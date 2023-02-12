/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "vk_test.h"

// only support on 64-bit, just because it's easier to share CPU & GPU structs if pointer size is
// identical

#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) || \
    defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)

RD_TEST(VK_KHR_Buffer_Address, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Test capture and replay of VK_KHR_buffer_device_address";

  // should match definition below in GLSL
  struct DrawData
  {
    DefaultA2V *vert_data;
    // no alignment on Vec4f, use scalar block layout
    Vec4f tint;
    Vec2f offset;
    Vec2f scale;
    // padding to make the struct size 16 to make aligning the buffer easier.
    Vec2f padding;
  };

  std::string common = R"EOSHADER(

#version 460 core

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
};

struct DefaultA2V {
  vec3 pos;
  vec4 col;
  vec2 uv;
};

layout(buffer_reference, scalar, buffer_reference_align = 16) buffer TriangleData {
  DefaultA2V verts[3];
};

layout(buffer_reference, scalar, buffer_reference_align = 16) buffer DrawData {
  TriangleData tri;
  vec4 tint;
  vec2 offset;
  vec2 scale;
  vec2 padding;
};

layout(push_constant) uniform PushData {
  DrawData data_ptr;
} push;

)EOSHADER";

  const std::string vertex = R"EOSHADER(

layout(location = 0) out v2f vertOut;

void main()
{
  DrawData draw = push.data_ptr;
  DefaultA2V vert = draw.tri.verts[gl_VertexIndex];

	gl_Position = vertOut.pos = vec4(vert.pos*vec3(draw.scale,1) + vec3(draw.offset, 0), 1);
	vertOut.col = vert.col;
	vertOut.uv = vec4(vert.uv, 0, 1);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  DrawData draw = push.data_ptr;

	Color = vertIn.col * draw.tint;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    devExts.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufaddrFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
    };

    getPhysFeatures2(&bufaddrFeatures);

    if(!bufaddrFeatures.bufferDeviceAddress)
      Avail = "Buffer device address feature 'bufferDeviceAddress' not available";

    bufaddrFeatures.bufferDeviceAddressCaptureReplay = 0;
    bufaddrFeatures.bufferDeviceAddressMultiDevice = 0;

    devInfoNext = &bufaddrFeatures;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(
        vkh::PipelineLayoutCreateInfo({}, {vkh::PushConstantRange(VK_SHADER_STAGE_ALL, 0, 8)}));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    std::vector<uint32_t> pixel_spirv = {
        0x07230203, 0x00010600, 0x000e0000, 0x00000035, 0x00000000, 0x00020011, 0x00000001,
        0x00020011, 0x000014e3, 0x0009000a, 0x5f565053, 0x5f52484b, 0x73796870, 0x6c616369,
        0x6f74735f, 0x65676172, 0x6675625f, 0x00726566, 0x0003000e, 0x000014e4, 0x00000001,
        0x000a000f, 0x00000004, 0x00000001, 0x6e69616d, 0x00000000, 0x00000002, 0x00000003,
        0x00000004, 0x00000005, 0x00000006, 0x00030010, 0x00000001, 0x00000007, 0x00030003,
        0x00000005, 0x00000258, 0x00090005, 0x00000007, 0x65707974, 0x7375502e, 0x6e6f4368,
        0x6e617473, 0x75502e74, 0x61446873, 0x00006174, 0x00060006, 0x00000007, 0x00000000,
        0x61746164, 0x7274705f, 0x00000000, 0x00050005, 0x00000008, 0x77617244, 0x61746144,
        0x00000000, 0x00040006, 0x00000008, 0x00000000, 0x00697274, 0x00050006, 0x00000008,
        0x00000001, 0x746e6974, 0x00000000, 0x00050006, 0x00000008, 0x00000002, 0x7366666f,
        0x00007465, 0x00050006, 0x00000008, 0x00000003, 0x6c616373, 0x00000065, 0x00050006,
        0x00000008, 0x00000004, 0x64646170, 0x00676e69, 0x00060005, 0x00000009, 0x61697254,
        0x656c676e, 0x61746144, 0x00000000, 0x00050006, 0x00000009, 0x00000000, 0x74726576,
        0x00000073, 0x00050005, 0x0000000a, 0x61666544, 0x41746c75, 0x00005632, 0x00040006,
        0x0000000a, 0x00000000, 0x00736f70, 0x00040006, 0x0000000a, 0x00000001, 0x006c6f63,
        0x00040006, 0x0000000a, 0x00000002, 0x00007675, 0x00040005, 0x00000006, 0x68737570,
        0x00000000, 0x00060005, 0x00000002, 0x762e6e69, 0x432e7261, 0x524f4c4f, 0x00000030,
        0x00060005, 0x00000003, 0x762e6e69, 0x432e7261, 0x524f4c4f, 0x00000031, 0x00070005,
        0x00000004, 0x762e6e69, 0x542e7261, 0x4f435845, 0x3044524f, 0x00000000, 0x00070005,
        0x00000005, 0x2e74756f, 0x2e726176, 0x545f5653, 0x65677261, 0x00003074, 0x00040005,
        0x00000001, 0x6e69616d, 0x00000000, 0x00050005, 0x0000000b, 0x495f5350, 0x5455504e,
        0x00000000, 0x00040006, 0x0000000b, 0x00000000, 0x00736f70, 0x00040006, 0x0000000b,
        0x00000001, 0x006c6f63, 0x00040006, 0x0000000b, 0x00000002, 0x00007675, 0x00050005,
        0x0000000c, 0x4f5f5350, 0x55505455, 0x00000054, 0x00050006, 0x0000000c, 0x00000000,
        0x6c6f4376, 0x0000726f, 0x00040047, 0x00000002, 0x0000001e, 0x00000000, 0x00040047,
        0x00000003, 0x0000001e, 0x00000001, 0x00040047, 0x00000004, 0x0000001e, 0x00000002,
        0x00040047, 0x00000005, 0x0000001e, 0x00000000, 0x00050048, 0x0000000a, 0x00000000,
        0x00000023, 0x00000000, 0x00050048, 0x0000000a, 0x00000001, 0x00000023, 0x0000000c,
        0x00050048, 0x0000000a, 0x00000002, 0x00000023, 0x0000001c, 0x00040047, 0x0000000d,
        0x00000006, 0x00000024, 0x00050048, 0x00000009, 0x00000000, 0x00000023, 0x00000000,
        0x00030047, 0x00000009, 0x00000002, 0x00050048, 0x00000008, 0x00000000, 0x00000023,
        0x00000000, 0x00050048, 0x00000008, 0x00000001, 0x00000023, 0x00000008, 0x00050048,
        0x00000008, 0x00000002, 0x00000023, 0x00000018, 0x00050048, 0x00000008, 0x00000003,
        0x00000023, 0x00000020, 0x00050048, 0x00000008, 0x00000004, 0x00000023, 0x00000028,
        0x00030047, 0x00000008, 0x00000002, 0x00050048, 0x00000007, 0x00000000, 0x00000023,
        0x00000000, 0x00030047, 0x00000007, 0x00000002, 0x00030047, 0x0000000e, 0x000014ec,
        0x00040015, 0x0000000f, 0x00000020, 0x00000001, 0x0004002b, 0x0000000f, 0x00000010,
        0x00000000, 0x0004002b, 0x0000000f, 0x00000011, 0x00000001, 0x00040015, 0x00000012,
        0x00000020, 0x00000000, 0x0004002b, 0x00000012, 0x00000013, 0x00000003, 0x00030016,
        0x00000014, 0x00000020, 0x00040017, 0x00000015, 0x00000014, 0x00000003, 0x00040017,
        0x00000016, 0x00000014, 0x00000004, 0x00040017, 0x00000017, 0x00000014, 0x00000002,
        0x0005001e, 0x0000000a, 0x00000015, 0x00000016, 0x00000017, 0x0004001c, 0x0000000d,
        0x0000000a, 0x00000013, 0x0003001e, 0x00000009, 0x0000000d, 0x00040020, 0x00000018,
        0x000014e5, 0x00000009, 0x0007001e, 0x00000008, 0x00000018, 0x00000016, 0x00000017,
        0x00000017, 0x00000017, 0x00040020, 0x00000019, 0x000014e5, 0x00000008, 0x0003001e,
        0x00000007, 0x00000019, 0x00040020, 0x0000001a, 0x00000009, 0x00000007, 0x00040020,
        0x0000001b, 0x00000001, 0x00000016, 0x00040020, 0x0000001c, 0x00000003, 0x00000016,
        0x00020013, 0x0000001d, 0x00030021, 0x0000001e, 0x0000001d, 0x0005001e, 0x0000000b,
        0x00000016, 0x00000016, 0x00000016, 0x00040020, 0x0000001f, 0x00000007, 0x0000000b,
        0x0003001e, 0x0000000c, 0x00000016, 0x00040021, 0x00000020, 0x0000000c, 0x0000001f,
        0x00040020, 0x00000021, 0x00000007, 0x0000000c, 0x00040020, 0x00000022, 0x00000007,
        0x00000019, 0x00040020, 0x00000023, 0x00000009, 0x00000019, 0x00040020, 0x00000024,
        0x00000007, 0x00000016, 0x00040020, 0x00000025, 0x000014e5, 0x00000016, 0x0004003b,
        0x0000001a, 0x00000006, 0x00000009, 0x0004003b, 0x0000001b, 0x00000002, 0x00000001,
        0x0004003b, 0x0000001b, 0x00000003, 0x00000001, 0x0004003b, 0x0000001b, 0x00000004,
        0x00000001, 0x0004003b, 0x0000001c, 0x00000005, 0x00000003, 0x00030001, 0x00000016,
        0x00000026, 0x00050036, 0x0000001d, 0x00000001, 0x00000000, 0x0000001e, 0x000200f8,
        0x00000027, 0x0004003b, 0x00000024, 0x00000028, 0x00000007, 0x0004003b, 0x00000024,
        0x00000029, 0x00000007, 0x0004003b, 0x00000024, 0x0000002a, 0x00000007, 0x0004003b,
        0x00000022, 0x0000000e, 0x00000007, 0x0004003d, 0x00000016, 0x0000002b, 0x00000002,
        0x0004003d, 0x00000016, 0x0000002c, 0x00000003, 0x0004003d, 0x00000016, 0x0000002d,
        0x00000004, 0x00060050, 0x0000000b, 0x0000002e, 0x0000002b, 0x0000002c, 0x0000002d,
        0x0003003e, 0x00000028, 0x0000002c, 0x00050041, 0x00000023, 0x0000002f, 0x00000006,
        0x00000010, 0x0004003d, 0x00000019, 0x00000030, 0x0000002f, 0x0003003e, 0x0000000e,
        0x00000030, 0x00050041, 0x00000025, 0x00000031, 0x00000030, 0x00000011, 0x0006003d,
        0x00000016, 0x00000032, 0x00000031, 0x00000002, 0x00000004, 0x00050085, 0x00000016,
        0x00000033, 0x0000002c, 0x00000032, 0x0003003e, 0x0000002a, 0x00000033, 0x00040050,
        0x0000000c, 0x00000034, 0x00000033, 0x0003003e, 0x00000029, 0x00000033, 0x0003003e,
        0x00000005, 0x00000033, 0x000100fd, 0x00010038,
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CopyShaderModule(pixel_spirv, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    vkh::BufferCreateInfo bufinfo(0x100000, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR);

    VkMemoryAllocateInfo memoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkMemoryAllocateFlagsInfo memoryAllocateFlags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};

    memoryAllocateFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
    memoryAllocateInfo.pNext = &memoryAllocateFlags;

    const VkPhysicalDeviceMemoryProperties *memProps = NULL;
    vmaGetMemoryProperties(allocator, &memProps);

    VkBuffer databuf;
    vkCreateBuffer(device, bufinfo, NULL, &databuf);

    VkMemoryRequirements mrq;
    vkGetBufferMemoryRequirements(device, databuf, &mrq);

    memoryAllocateInfo.allocationSize = mrq.size;

    for(uint32_t i = 0; i < memProps->memoryTypeCount; i++)
    {
      if((mrq.memoryTypeBits & (1u << i)) &&
         (memProps->memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
      {
        memoryAllocateInfo.memoryTypeIndex = i;
        break;
      }
    }

    VkDeviceMemory databufMem;
    vkAllocateMemory(device, &memoryAllocateInfo, NULL, &databufMem);
    vkBindBufferMemory(device, databuf, databufMem, 0);

    // north-facing primary colours triangle
    const DefaultA2V tri1[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    // north-west-facing triangle
    const DefaultA2V tri2[3] = {
        {Vec3f(-0.5f, 0.5f, 0.0f), Vec4f(1.0f, 0.2f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.5f, 0.5f, 0.0f), Vec4f(0.7f, 0.85f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 1.0f, 0.4f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    VkBufferDeviceAddressInfoKHR info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
    info.buffer = databuf;

    VkDeviceAddress baseAddr = vkGetBufferDeviceAddressKHR(device, &info);
    byte *gpuptr = (byte *)baseAddr;    // not a valid cpu pointer but useful for avoiding casting

    byte *cpuptr = NULL;
    vkMapMemory(device, databufMem, 0, mrq.size, 0, (void **)&cpuptr);

    // put triangle data first
    memcpy(cpuptr, tri1, sizeof(tri1));
    DefaultA2V *gputri1 = (DefaultA2V *)gpuptr;
    cpuptr += sizeof(tri1);
    gpuptr += sizeof(tri1);

    // align to 16 bytes
    cpuptr = AlignUpPtr(cpuptr, 16);
    gpuptr = AlignUpPtr(gpuptr, 16);

    memcpy(cpuptr, tri2, sizeof(tri2));
    DefaultA2V *gputri2 = (DefaultA2V *)gpuptr;
    cpuptr += sizeof(tri2);
    gpuptr += sizeof(tri2);

    // align to 16 bytes
    cpuptr = AlignUpPtr(cpuptr, 16);
    gpuptr = AlignUpPtr(gpuptr, 16);

    DrawData *drawscpu = (DrawData *)cpuptr;
    DrawData *drawsgpu = (DrawData *)gpuptr;

    drawscpu[0].vert_data = gputri1;
    drawscpu[0].offset = Vec2f(-0.5f, 0.0f);
    drawscpu[0].scale = Vec2f(0.5f, 0.5f);
    drawscpu[0].tint = Vec4f(1.0f, 0.5f, 0.5f, 1.0f);    // tint red

    drawscpu[1].vert_data = gputri1;
    drawscpu[1].offset = Vec2f(0.0f, 0.0f);
    drawscpu[1].scale = Vec2f(0.5f, -0.5f);              // flip vertically
    drawscpu[1].tint = Vec4f(0.2f, 0.5f, 1.0f, 1.0f);    // tint blue

    drawscpu[2].vert_data = gputri2;    // use second triangle
    drawscpu[2].offset = Vec2f(0.6f, 0.0f);
    drawscpu[2].scale = Vec2f(0.5f, 0.5f);
    drawscpu[2].tint = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);

    float time = 0.0f;

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      for(int i = 0; i < 2; i++)
      {
        VkBuffer midbuf = VK_NULL_HANDLE;
        vkCreateBuffer(device, bufinfo, NULL, &midbuf);

        VkDeviceMemory midmem = VK_NULL_HANDLE;
        vkAllocateMemory(device, &memoryAllocateInfo, NULL, &midmem);
        vkBindBufferMemory(device, midbuf, midmem, 0);

        vkMapMemory(device, midmem, 0, mrq.size, 0, (void **)&cpuptr);
        vkDestroyBuffer(device, midbuf, NULL);
        vkUnmapMemory(device, midmem);
        vkFreeMemory(device, midmem, NULL);
      }

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);

      // look ma, no binds
      DrawData *bindptr = drawsgpu;
      drawscpu[0].scale.x = (abs(sinf(time)) + 0.1f) * 0.5f;
      drawscpu[0].scale.y = 0.5f;
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, 8, &bindptr);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      bindptr++;
      drawscpu[1].scale.x = 0.5f;
      drawscpu[1].scale.y = (abs(cosf(time)) + 0.1f) * 0.5f;
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, 8, &bindptr);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      bindptr++;
      drawscpu[2].scale = Vec2f(0.5f, 0.5f);
      drawscpu[2].tint = Vec4f(cosf(time) * 0.5f + 0.5f, sinf(time) * 0.5f + 0.5f,
                               cosf(time + 3.14f) * 0.5f + 0.5f, 1.0f);
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, 8, &bindptr);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      vkDeviceWaitIdle(device);

      drawscpu[0].scale = Vec2f(0.0f, 0.0f);
      drawscpu[1].scale = Vec2f(0.0f, 0.0f);
      drawscpu[2].scale = Vec2f(0.0f, 0.0f);

      Present();

      time += 0.1f;
    }

    CHECK_VKR(vkDeviceWaitIdle(device));

    vkDestroyBuffer(device, databuf, NULL);
    vkUnmapMemory(device, databufMem);
    vkFreeMemory(device, databufMem, NULL);

    return 0;
  }
};

REGISTER_TEST();

#endif    // if 64-bit
