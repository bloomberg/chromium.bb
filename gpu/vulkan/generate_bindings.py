#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for Vulkan function pointers."""

import optparse
import os
import platform
import sys
from subprocess import call

VULKAN_UNASSOCIATED_FUNCTIONS = [
# vkGetInstanceProcAddr belongs here but is handled specially.
{ 'name': 'vkCreateInstance' },
{ 'name': 'vkEnumerateInstanceExtensionProperties' },
{ 'name': 'vkEnumerateInstanceLayerProperties' },
]

VULKAN_INSTANCE_FUNCTIONS = [
{ 'name': 'vkDestroyInstance' },
# vkDestroySurfaceKHR belongs here but is handled specially.
{ 'name': 'vkEnumeratePhysicalDevices' },
{ 'name': 'vkGetDeviceProcAddr' },
]

VULKAN_PHYSICAL_DEVICE_FUNCTIONS = [
{ 'name': 'vkCreateDevice' },
{ 'name': 'vkEnumerateDeviceLayerProperties' },
{ 'name': 'vkGetPhysicalDeviceQueueFamilyProperties' },
# The following functions belong here but are handled specially:
# vkGetPhysicalDeviceSurfaceCapabilitiesKHR
# vkGetPhysicalDeviceSurfaceFormatsKHR
# vkGetPhysicalDeviceSurfaceSupportKHR
]

VULKAN_DEVICE_FUNCTIONS = [
{ 'name': 'vkAllocateCommandBuffers' },
{ 'name': 'vkAllocateDescriptorSets' },
{ 'name': 'vkCreateCommandPool' },
{ 'name': 'vkCreateDescriptorPool' },
{ 'name': 'vkCreateDescriptorSetLayout' },
{ 'name': 'vkCreateFence' },
{ 'name': 'vkCreateFramebuffer' },
{ 'name': 'vkCreateImageView' },
{ 'name': 'vkCreateRenderPass' },
{ 'name': 'vkCreateSampler' },
{ 'name': 'vkCreateSemaphore' },
{ 'name': 'vkCreateShaderModule' },
{ 'name': 'vkDestroyCommandPool' },
{ 'name': 'vkDestroyDescriptorPool' },
{ 'name': 'vkDestroyDescriptorSetLayout' },
{ 'name': 'vkDestroyDevice' },
{ 'name': 'vkDestroyFramebuffer' },
{ 'name': 'vkDestroyFence' },
{ 'name': 'vkDestroyImage' },
{ 'name': 'vkDestroyImageView' },
{ 'name': 'vkDestroyRenderPass' },
{ 'name': 'vkDestroySampler' },
{ 'name': 'vkDestroySemaphore' },
{ 'name': 'vkDestroyShaderModule' },
{ 'name': 'vkFreeCommandBuffers' },
{ 'name': 'vkFreeDescriptorSets' },
{ 'name': 'vkFreeMemory' },
{ 'name': 'vkGetDeviceQueue' },
{ 'name': 'vkGetFenceStatus' },
{ 'name': 'vkResetFences' },
{ 'name': 'vkUpdateDescriptorSets' },
{ 'name': 'vkWaitForFences' },
]

VULKAN_QUEUE_FUNCTIONS = [
{ 'name': 'vkQueueSubmit' },
{ 'name': 'vkQueueWaitIdle' },
]

VULKAN_COMMAND_BUFFER_FUNCTIONS = [
{ 'name': 'vkBeginCommandBuffer' },
{ 'name': 'vkCmdBeginRenderPass' },
{ 'name': 'vkCmdEndRenderPass' },
{ 'name': 'vkCmdExecuteCommands' },
{ 'name': 'vkCmdNextSubpass' },
{ 'name': 'vkCmdPipelineBarrier' },
{ 'name': 'vkEndCommandBuffer' },
{ 'name': 'vkResetCommandBuffer' },
]

VULKAN_SWAPCHAIN_FUNCTIONS = [
{ 'name': 'vkAcquireNextImageKHR' },
{ 'name': 'vkCreateSwapchainKHR' },
{ 'name': 'vkDestroySwapchainKHR' },
{ 'name': 'vkGetSwapchainImagesKHR' },
{ 'name': 'vkQueuePresentKHR' },
]

SELF_LOCATION = os.path.dirname(os.path.abspath(__file__))

LICENSE_AND_HEADER = """\
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

"""

def GenerateHeaderFile(file, unassociated_functions, instance_functions,
                       physical_device_functions, device_functions,
                       queue_functions, command_buffer_functions,
                       swapchain_functions):
  """Generates gpu/vulkan/vulkan_function_pointers.h"""

  file.write(LICENSE_AND_HEADER +
"""

#ifndef GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
#define GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_

#include <vulkan/vulkan.h>

#include "base/native_library.h"
#include "gpu/vulkan/vulkan_export.h"

namespace gpu {

struct VulkanFunctionPointers;

VULKAN_EXPORT VulkanFunctionPointers* GetVulkanFunctionPointers();

struct VulkanFunctionPointers {
  VulkanFunctionPointers();
  ~VulkanFunctionPointers();

  bool BindUnassociatedFunctionPointers();

  // These functions assume that vkGetInstanceProcAddr has been populated.
  bool BindInstanceFunctionPointers(VkInstance vk_instance);
  bool BindPhysicalDeviceFunctionPointers(VkInstance vk_instance);

  // These functions assume that vkGetDeviceProcAddr has been populated.
  bool BindDeviceFunctionPointers(VkDevice vk_device);
  bool BindSwapchainFunctionPointers(VkDevice vk_device);

  base::NativeLibrary vulkan_loader_library_ = nullptr;

  // Unassociated functions
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
""")

  for func in unassociated_functions:
    file.write('  PFN_' + func['name'] + ' ' + func['name'] + ' = nullptr;\n')

  file.write("""\

  // Instance functions
""")

  for func in instance_functions:
    file.write('  PFN_' + func['name'] + ' ' + func['name'] + ' = nullptr;\n')

  file.write("""\
  PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;

  // Physical Device functions
""")

  for func in physical_device_functions:
    file.write('  PFN_' + func['name'] + ' ' + func['name'] + ' = nullptr;\n')

  file.write("""\
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
      vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR
      vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;

  // Device functions
""")

  for func in device_functions:
    file.write('  PFN_' + func['name'] + ' ' + func['name'] + ' = nullptr;\n')

  file.write("""\

  // Queue functions
""")

  for func in queue_functions:
    file.write('  PFN_' + func['name'] + ' ' + func['name'] + ' = nullptr;\n')

  file.write("""\

  // Command Buffer functions
""")

  for func in command_buffer_functions:
    file.write('  PFN_' + func['name'] + ' ' + func['name'] + ' = nullptr;\n')

  file.write("""\

  // Swapchain functions
""")

  for func in swapchain_functions:
    file.write('  PFN_' + func['name'] + ' ' + func['name'] + ' = nullptr;\n')

  file.write("""\
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
""")

def GenerateSourceFile(file, unassociated_functions, instance_functions,
                       physical_device_functions, device_functions,
                       queue_functions, command_buffer_functions,
                       swapchain_functions):
  """Generates gpu/vulkan/vulkan_function_pointers.cc"""

  file.write(LICENSE_AND_HEADER +
"""

#include "gpu/vulkan/vulkan_function_pointers.h"

#include "base/no_destructor.h"

namespace gpu {

VulkanFunctionPointers* GetVulkanFunctionPointers() {
  static base::NoDestructor<VulkanFunctionPointers> vulkan_function_pointers;
  return vulkan_function_pointers.get();
}

VulkanFunctionPointers::VulkanFunctionPointers() = default;
VulkanFunctionPointers::~VulkanFunctionPointers() = default;

bool VulkanFunctionPointers::BindUnassociatedFunctionPointers() {
  // vkGetInstanceProcAddr must be handled specially since it gets its function
  // pointer through base::GetFunctionPOinterFromNativeLibrary(). Other Vulkan
  // functions don't do this.
  vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(vulkan_loader_library_,
                                                "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddr)
    return false;

""")

  for func in unassociated_functions:
    file.write('  ' + func['name'] + ' = reinterpret_cast<PFN_' + func['name']
      + '>(vkGetInstanceProcAddr(nullptr, "' + func['name'] + '"));\n')
    file.write('  if (!' + func['name'] + ')\n')
    file.write('    return false;\n\n')

  file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindInstanceFunctionPointers(
    VkInstance vk_instance) {
""")

  for func in instance_functions:
    file.write('  ' + func['name'] + ' = reinterpret_cast<PFN_' + func['name']
      + '>(vkGetInstanceProcAddr(vk_instance, "' + func['name'] + '"));\n')
    file.write('  if (!' + func['name'] + ')\n')
    file.write('    return false;\n\n')

  file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindPhysicalDeviceFunctionPointers(
    VkInstance vk_instance) {
""")

  for func in physical_device_functions:
    file.write('  ' + func['name'] + ' = reinterpret_cast<PFN_' + func['name']
      + '>(vkGetInstanceProcAddr(vk_instance, "' + func['name'] + '"));\n')
    file.write('  if (!' + func['name'] + ')\n')
    file.write('    return false;\n\n')

  file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindDeviceFunctionPointers(VkDevice vk_device) {
  // Device functions
""")
  for func in device_functions:
    file.write('  ' + func['name'] + ' = reinterpret_cast<PFN_' + func['name'] +
      '>(vkGetDeviceProcAddr(vk_device, "' + func['name'] + '"));\n')
    file.write('  if (!' + func['name'] + ')\n')
    file.write('    return false;\n\n')

  file.write("""\

  // Queue functions
""")
  for func in queue_functions:
    file.write('  ' + func['name'] + ' = reinterpret_cast<PFN_' + func['name'] +
      '>(vkGetDeviceProcAddr(vk_device, "' + func['name'] + '"));\n')
    file.write('  if (!' + func['name'] + ')\n')
    file.write('    return false;\n\n')

  file.write("""\

  // Command Buffer functions
""")
  for func in command_buffer_functions:
    file.write('  ' + func['name'] + ' = reinterpret_cast<PFN_' + func['name'] +
      '>(vkGetDeviceProcAddr(vk_device, "' + func['name'] + '"));\n')
    file.write('  if (!' + func['name'] + ')\n')
    file.write('    return false;\n\n')

  file.write("""\


  return true;
}

bool VulkanFunctionPointers::BindSwapchainFunctionPointers(VkDevice vk_device) {
""")

  for func in swapchain_functions:
    file.write('  ' + func['name'] + ' = reinterpret_cast<PFN_' + func['name'] +
      '>(vkGetDeviceProcAddr(vk_device, "' + func['name'] + '"));\n')
    file.write('  if (!' + func['name'] + ')\n')
    file.write('    return false;\n\n')

  file.write("""\


  return true;
}

}  // namespace gpu
""")

def main(argv):
  """This is the main function."""

  parser = optparse.OptionParser()
  _, args = parser.parse_args(argv)

  directory = SELF_LOCATION
  if len(args) >= 1:
    directory = args[0]

  def ClangFormat(filename):
    formatter = "clang-format"
    if platform.system() == "Windows":
      formatter += ".bat"
    call([formatter, "-i", "-style=chromium", filename])

  header_file = open(
      os.path.join(directory, 'vulkan_function_pointers.h'), 'wb')
  GenerateHeaderFile(header_file, VULKAN_UNASSOCIATED_FUNCTIONS,
                     VULKAN_INSTANCE_FUNCTIONS,
                     VULKAN_PHYSICAL_DEVICE_FUNCTIONS, VULKAN_DEVICE_FUNCTIONS,
                     VULKAN_QUEUE_FUNCTIONS, VULKAN_COMMAND_BUFFER_FUNCTIONS,
                     VULKAN_SWAPCHAIN_FUNCTIONS)
  header_file.close()
  ClangFormat(header_file.name)

  source_file = open(
      os.path.join(directory, 'vulkan_function_pointers.cc'), 'wb')
  GenerateSourceFile(source_file, VULKAN_UNASSOCIATED_FUNCTIONS,
                     VULKAN_INSTANCE_FUNCTIONS,
                     VULKAN_PHYSICAL_DEVICE_FUNCTIONS, VULKAN_DEVICE_FUNCTIONS,
                     VULKAN_QUEUE_FUNCTIONS, VULKAN_COMMAND_BUFFER_FUNCTIONS,
                     VULKAN_SWAPCHAIN_FUNCTIONS)
  source_file.close()
  ClangFormat(source_file.name)

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
