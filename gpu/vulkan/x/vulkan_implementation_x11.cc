// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/x/vulkan_implementation_x11.h"

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/optional.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_posix_util.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_util.h"
#include "gpu/vulkan/x/vulkan_surface_x11.h"
#include "ui/gfx/gpu_fence.h"

namespace gpu {

namespace {

class ScopedUnsetDisplay {
 public:
  ScopedUnsetDisplay() : display_(getenv("DISPLAY")) { unsetenv("DISPLAY"); }
  ~ScopedUnsetDisplay() { setenv("DISPLAY", display_.c_str(), 1); }

 private:
  std::string display_;
  DISALLOW_COPY_AND_ASSIGN(ScopedUnsetDisplay);
};

}  // namespace

VulkanImplementationX11::VulkanImplementationX11() {
  gfx::GetXDisplay();
}

VulkanImplementationX11::~VulkanImplementationX11() {}

bool VulkanImplementationX11::InitializeVulkanInstance(bool using_surface) {
  using_surface_ = using_surface;
  // Unset DISPLAY env, so the vulkan can be initialized successfully, if the X
  // server doesn't support Vulkan surface.
  base::Optional<ScopedUnsetDisplay> unset_display;
  if (!using_surface_)
    unset_display.emplace();

  std::vector<const char*> required_extensions = {
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME};
  if (using_surface_) {
    required_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    required_extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
  }

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  base::NativeLibraryLoadError native_library_load_error;
  vulkan_function_pointers->vulkan_loader_library_ = base::LoadNativeLibrary(
      base::FilePath("libvulkan.so.1"), &native_library_load_error);
  if (!vulkan_function_pointers->vulkan_loader_library_)
    return false;

  if (!vulkan_instance_.Initialize(required_extensions, {}))
    return false;
  return true;
}

VulkanInstance* VulkanImplementationX11::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<VulkanSurface> VulkanImplementationX11::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  DLOG_IF(FATAL, !using_surface_)
      << "Flag --disable-vulkan-surface is provided.";
  return VulkanSurfaceX11::Create(vulkan_instance_.vk_instance(), window);
}

bool VulkanImplementationX11::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  XDisplay* display = gfx::GetXDisplay();
  return vkGetPhysicalDeviceXlibPresentationSupportKHR(
      device, queue_family_index, display,
      XVisualIDFromVisual(DefaultVisual(display, DefaultScreen(display))));
}

std::vector<const char*>
VulkanImplementationX11::GetRequiredDeviceExtensions() {
  std::vector<const char*> extensions = {
      VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME};
  if (using_surface_)
    extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  return extensions;
}

VkFence VulkanImplementationX11::CreateVkFenceForGpuFence(VkDevice vk_device) {
  NOTREACHED();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence> VulkanImplementationX11::ExportVkFenceToGpuFence(
    VkDevice vk_device,
    VkFence vk_fence) {
  NOTREACHED();
  return nullptr;
}

VkSemaphore VulkanImplementationX11::CreateExternalSemaphore(
    VkDevice vk_device) {
  return CreateExternalVkSemaphore(
      vk_device, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
}

VkSemaphore VulkanImplementationX11::ImportSemaphoreHandle(
    VkDevice vk_device,
    SemaphoreHandle sync_handle) {
  return ImportVkSemaphoreHandlePosix(vk_device, std::move(sync_handle));
}

SemaphoreHandle VulkanImplementationX11::GetSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore) {
  return GetVkSemaphoreHandlePosix(
      vk_device, vk_semaphore, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
}

VkExternalMemoryHandleTypeFlagBits
VulkanImplementationX11::GetExternalImageHandleType() {
  return VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
}

}  // namespace gpu
