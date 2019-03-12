// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"

#include <utility>

#include "android_webview/public/browser/draw_fn.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/vk/GrVkBackendContext.h"
#include "third_party/skia/include/gpu/vk/GrVkExtensions.h"

namespace android_webview {

namespace {

AwVulkanContextProvider* g_vulkan_context_provider = nullptr;

GrVkGetProc MakeUnifiedGetter(const PFN_vkGetInstanceProcAddr& iproc,
                              const PFN_vkGetDeviceProcAddr& dproc) {
  return [&iproc, &dproc](const char* proc_name, VkInstance instance,
                          VkDevice device) {
    if (device != VK_NULL_HANDLE) {
      return dproc(device, proc_name);
    }
    return iproc(instance, proc_name);
  };
}

bool InitVulkanForWebView(VkInstance instance, VkDevice device) {
  gpu::VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  // If we are re-initing, we don't need to re-load the shared library or
  // re-bind unassociated pointers. These shouldn't change.
  if (!vulkan_function_pointers->vulkan_loader_library_) {
    base::NativeLibraryLoadError native_library_load_error;
    vulkan_function_pointers->vulkan_loader_library_ = base::LoadNativeLibrary(
        base::FilePath("libvulkan.so"), &native_library_load_error);
    if (!vulkan_function_pointers->vulkan_loader_library_)
      return false;
    if (!vulkan_function_pointers->BindUnassociatedFunctionPointers())
      return false;
  }

  // These vars depend on |instance| and |device| and should be
  // re-initialized.
  if (!vulkan_function_pointers->BindInstanceFunctionPointers(instance))
    return false;
  if (!vulkan_function_pointers->BindPhysicalDeviceFunctionPointers(instance))
    return false;
  if (!vulkan_function_pointers->BindDeviceFunctionPointers(device))
    return false;

  return true;
}

}  // namespace

// static
scoped_refptr<AwVulkanContextProvider>
AwVulkanContextProvider::GetOrCreateInstance(AwDrawFn_InitVkParams* params) {
  if (g_vulkan_context_provider) {
    DCHECK_EQ(params->device, g_vulkan_context_provider->device());
    DCHECK_EQ(params->queue, g_vulkan_context_provider->queue());
    return base::WrapRefCounted(g_vulkan_context_provider);
  }

  auto provider = base::WrapRefCounted(new AwVulkanContextProvider);
  if (!provider->Initialize(params))
    return nullptr;

  return provider;
}

AwVulkanContextProvider::AwVulkanContextProvider() {
  DCHECK_EQ(nullptr, g_vulkan_context_provider);
  g_vulkan_context_provider = this;
}

AwVulkanContextProvider::~AwVulkanContextProvider() {
  DCHECK_EQ(g_vulkan_context_provider, this);
  g_vulkan_context_provider = nullptr;
  device_queue_->Destroy();
  device_queue_ = nullptr;
}

gpu::VulkanImplementation* AwVulkanContextProvider::GetVulkanImplementation() {
  return implementation_.get();
}

gpu::VulkanDeviceQueue* AwVulkanContextProvider::GetDeviceQueue() {
  return device_queue_.get();
}

GrContext* AwVulkanContextProvider::GetGrContext() {
  return gr_context_.get();
}

GrVkSecondaryCBDrawContext*
AwVulkanContextProvider::GetGrSecondaryCBDrawContext() {
  return draw_context_;
}

bool AwVulkanContextProvider::Initialize(AwDrawFn_InitVkParams* params) {
  // Don't call init on implementation. Instead call InitVulkanForWebView,
  // which avoids creating a new instance.
  implementation_ = gpu::CreateVulkanImplementation();
  if (!InitVulkanForWebView(params->instance, params->device)) {
    LOG(ERROR) << "Unable to initialize Vulkan pointers.";
    return false;
  }

  device_queue_ = std::make_unique<gpu::VulkanDeviceQueue>(params->instance);
  gfx::ExtensionSet extensions;
  for (uint32_t i = 0; i < params->enabled_device_extension_names_length; ++i)
    extensions.insert(params->enabled_device_extension_names[i]);
  device_queue_->InitializeForWevbView(
      params->physical_device, params->device, params->queue,
      params->graphics_queue_index, std::move(extensions));

  // Create our Skia GrContext.
  GrVkGetProc get_proc =
      MakeUnifiedGetter(vkGetInstanceProcAddr, vkGetDeviceProcAddr);
  GrVkExtensions vk_extensions;
  vk_extensions.init(get_proc, params->instance, params->physical_device,
                     params->enabled_instance_extension_names_length,
                     params->enabled_instance_extension_names,
                     params->enabled_device_extension_names_length,
                     params->enabled_device_extension_names);
  GrVkBackendContext backend_context{
      .fInstance = params->instance,
      .fPhysicalDevice = params->physical_device,
      .fDevice = params->device,
      .fQueue = params->queue,
      .fGraphicsQueueIndex = params->graphics_queue_index,
      .fMaxAPIVersion = params->api_version,
      .fVkExtensions = &vk_extensions,
      .fDeviceFeatures = params->device_features,
      .fDeviceFeatures2 = params->device_features_2,
      .fMemoryAllocator = nullptr,
      .fGetProc = get_proc,
      .fOwnsInstanceAndDevice = false,
  };
  gr_context_ = GrContext::MakeVulkan(backend_context);
  if (!gr_context_) {
    LOG(ERROR) << "Unable to initialize GrContext.";
    return false;
  }
  return true;
}

}  // namespace android_webview
