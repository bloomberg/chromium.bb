// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include <d3d12.h>

#include "base/file_version_info_win.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_native_library.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/vulkan/include/vulkan/vulkan.h"

namespace gpu {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum D3DFeatureLevel in \tools\metrics\histograms\enums.xml
enum class D3D12FeatureLevel {
  kD3DFeatureLevelUnknown = 0,
  kD3DFeatureLevel_12_0 = 1,
  kD3DFeatureLevel_12_1 = 2,
  kMaxValue = kD3DFeatureLevel_12_1,
};

inline D3D12FeatureLevel ConvertToHistogramFeatureLevel(
    uint32_t d3d_feature_level) {
  switch (d3d_feature_level) {
    case 0:
      return D3D12FeatureLevel::kD3DFeatureLevelUnknown;
    case D3D_FEATURE_LEVEL_12_0:
      return D3D12FeatureLevel::kD3DFeatureLevel_12_0;
    case D3D_FEATURE_LEVEL_12_1:
      return D3D12FeatureLevel::kD3DFeatureLevel_12_1;
    default:
      NOTREACHED();
      return D3D12FeatureLevel::kD3DFeatureLevelUnknown;
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum VulkanVersion in \tools\metrics\histograms\enums.xml
enum class VulkanVersion {
  kVulkanVersionUnknown = 0,
  kVulkanVersion_1_0_0 = 1,
  kVulkanVersion_1_1_0 = 2,
  kMaxValue = kVulkanVersion_1_1_0,
};

inline VulkanVersion ConvertToHistogramVulkanVersion(uint32_t vulkan_version) {
  switch (vulkan_version) {
    case 0:
      return VulkanVersion::kVulkanVersionUnknown;
    case VK_MAKE_VERSION(1, 0, 0):
      return VulkanVersion::kVulkanVersion_1_0_0;
    case VK_MAKE_VERSION(1, 1, 0):
      return VulkanVersion::kVulkanVersion_1_1_0;
    default:
      NOTREACHED();
      return VulkanVersion::kVulkanVersionUnknown;
  }
}

}  // namespace anonymous

// DirectX 12 are included with Windows 10 and Server 2016.
void GetGpuSupportedD3D12Version(GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "GetGpuSupportedD3D12Version");
  gpu_info->supports_dx12 = false;
  gpu_info->d3d12_feature_level = 0;

  base::NativeLibrary d3d12_library =
      base::LoadNativeLibrary(base::FilePath(L"d3d12.dll"), nullptr);
  if (!d3d12_library) {
    return;
  }

  // The order of feature levels to attempt to create in D3D CreateDevice
  const D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_12_1,
                                              D3D_FEATURE_LEVEL_12_0};

  PFN_D3D12_CREATE_DEVICE D3D12CreateDevice =
      reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
          GetProcAddress(d3d12_library, "D3D12CreateDevice"));
  if (D3D12CreateDevice) {
    // For the default adapter only. (*pAdapter == nullptr)
    // Check to see if the adapter supports Direct3D 12, but don't create the
    // actual device yet. (**ppDevice == nullptr)
    for (auto level : feature_levels) {
      if (SUCCEEDED(D3D12CreateDevice(nullptr, level, _uuidof(ID3D12Device),
                                      nullptr))) {
        gpu_info->d3d12_feature_level = level;
        gpu_info->supports_dx12 = true;
        break;
      }
    }
  }

  base::UnloadNativeLibrary(d3d12_library);
}

bool BadAMDVulkanDriverVersion(GPUInfo* gpu_info) {
  bool secondary_gpu_amd = false;
  for (size_t i = 0; i < gpu_info->secondary_gpus.size(); ++i) {
    if (gpu_info->secondary_gpus[i].vendor_id == 0x1002) {
      secondary_gpu_amd = true;
      break;
    }
  }

  // Check both primary and seconday
  if (gpu_info->gpu.vendor_id == 0x1002 || secondary_gpu_amd) {
    std::unique_ptr<FileVersionInfoWin> file_version_info(
        static_cast<FileVersionInfoWin*>(
            FileVersionInfoWin::CreateFileVersionInfo(
                base::FilePath(FILE_PATH_LITERAL("amdvlk64.dll")))));

    if (file_version_info) {
      const int major =
          HIWORD(file_version_info->fixed_file_info()->dwFileVersionMS);
      const int minor =
          LOWORD(file_version_info->fixed_file_info()->dwFileVersionMS);
      const int minor_1 =
          HIWORD(file_version_info->fixed_file_info()->dwFileVersionLS);

      // From the Canary crash logs, the broken amdvlk64.dll versions
      // are 1.0.39.0, 1.0.51.0 and 1.0.54.0. In the manual test, version
      // 9.2.10.1 dated 12/6/2017 works and version 1.0.54.0 dated 11/2/1017
      // crashes. All version numbers small than 1.0.54.0 will be marked as
      // broken.
      if (major == 1 && minor == 0 && minor_1 <= 54) {
        return true;
      }
    }
  }

  return false;
}

bool InitVulkan(base::NativeLibrary* vulkan_library,
                PFN_vkGetInstanceProcAddr* vkGetInstanceProcAddr,
                PFN_vkCreateInstance* vkCreateInstance) {
  *vulkan_library =
      base::LoadNativeLibrary(base::FilePath(L"vulkan-1.dll"), nullptr);

  if (!(*vulkan_library)) {
    return false;
  }

  *vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      GetProcAddress(*vulkan_library, "vkGetInstanceProcAddr"));

  if (*vkGetInstanceProcAddr) {
    *vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        (*vkGetInstanceProcAddr)(nullptr, "vkCreateInstance"));
    if (*vkCreateInstance) {
      return true;
    }
  }
  base::UnloadNativeLibrary(*vulkan_library);
  return false;
}

bool InitVulkanInstanceProc(
    const VkInstance& vk_instance,
    PFN_vkGetInstanceProcAddr& vkGetInstanceProcAddr,
    PFN_vkDestroyInstance* vkDestroyInstance,
    PFN_vkEnumeratePhysicalDevices* vkEnumeratePhysicalDevices,
    PFN_vkEnumerateDeviceExtensionProperties*
        vkEnumerateDeviceExtensionProperties) {
  *vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
      vkGetInstanceProcAddr(vk_instance, "vkDestroyInstance"));

  *vkEnumeratePhysicalDevices =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          vkGetInstanceProcAddr(vk_instance, "vkEnumeratePhysicalDevices"));

  *vkEnumerateDeviceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkEnumerateDeviceExtensionProperties"));

  if ((*vkDestroyInstance) && (*vkEnumeratePhysicalDevices) &&
      (*vkEnumerateDeviceExtensionProperties)) {
    return true;
  }
  return false;
}

void GetGpuSupportedVulkanVersionAndExtensions(
    GPUInfo* gpu_info,
    const std::vector<const char*>& requested_vulkan_extensions,
    std::vector<bool>* extension_support) {
  TRACE_EVENT0("gpu", "GetGpuSupportedVulkanVersionAndExtensions");

  base::NativeLibrary vulkan_library;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkCreateInstance vkCreateInstance;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
  PFN_vkDestroyInstance vkDestroyInstance;
  VkInstance vk_instance = VK_NULL_HANDLE;
  uint32_t physical_device_count = 0;
  gpu_info->supports_vulkan = false;
  gpu_info->vulkan_version = 0;

  // Skip if the system has an older AMD Vulkan driver amdvlk64.dll which
  // crashes when vkCreateInstance() gets called. This bug is fixed in the
  // latest driver.
  if (BadAMDVulkanDriverVersion(gpu_info)) {
    return;
  }

  if (!InitVulkan(&vulkan_library, &vkGetInstanceProcAddr, &vkCreateInstance)) {
    return;
  }

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

  // Get the Vulkan API version supported in the GPU driver
  for (int minor_version = 1; minor_version >= 0; --minor_version) {
    app_info.apiVersion = VK_MAKE_VERSION(1, minor_version, 0);
    VkResult result = vkCreateInstance(&create_info, nullptr, &vk_instance);
    if (result == VK_SUCCESS && vk_instance &&
        InitVulkanInstanceProc(vk_instance, vkGetInstanceProcAddr,
                               &vkDestroyInstance, &vkEnumeratePhysicalDevices,
                               &vkEnumerateDeviceExtensionProperties)) {
      result = vkEnumeratePhysicalDevices(vk_instance, &physical_device_count,
                                          nullptr);
      if (result == VK_SUCCESS && physical_device_count > 0) {
        gpu_info->supports_vulkan = true;
        gpu_info->vulkan_version = app_info.apiVersion;
        break;
      } else {
        vkDestroyInstance(vk_instance, nullptr);
        vk_instance = VK_NULL_HANDLE;
      }
    }
  }

  // Check whether the requested_vulkan_extensions are supported
  if (gpu_info->supports_vulkan) {
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count,
                               physical_devices.data());

    // physical_devices[0]: Only query the default device for now
    uint32_t property_count;
    vkEnumerateDeviceExtensionProperties(physical_devices[0], nullptr,
                                         &property_count, nullptr);

    std::vector<VkExtensionProperties> extension_properties(property_count);
    if (property_count > 0) {
      vkEnumerateDeviceExtensionProperties(physical_devices[0], nullptr,
                                           &property_count,
                                           extension_properties.data());
    }

    for (size_t i = 0; i < requested_vulkan_extensions.size(); ++i) {
      for (size_t p = 0; p < property_count; ++p) {
        if (strcmp(requested_vulkan_extensions[i],
                   extension_properties[p].extensionName) == 0) {
          (*extension_support)[i] = true;
          break;
        }
      }
    }
  }

  if (vk_instance) {
    vkDestroyInstance(vk_instance, nullptr);
  }

  base::UnloadNativeLibrary(vulkan_library);
}

void RecordGpuSupportedRuntimeVersionHistograms(GPUInfo* gpu_info) {
  // D3D
  GetGpuSupportedD3D12Version(gpu_info);
  UMA_HISTOGRAM_BOOLEAN("GPU.SupportsDX12", gpu_info->supports_dx12);
  UMA_HISTOGRAM_ENUMERATION(
      "GPU.D3D12FeatureLevel",
      ConvertToHistogramFeatureLevel(gpu_info->d3d12_feature_level));

  // Vulkan
  const std::vector<const char*> vulkan_extensions = {
      "VK_KHR_external_memory_win32", "VK_KHR_external_semaphore_win32",
      "VK_KHR_win32_keyed_mutex"};
  std::vector<bool> extension_support(vulkan_extensions.size(), false);
  GetGpuSupportedVulkanVersionAndExtensions(gpu_info, vulkan_extensions,
                                            &extension_support);

  UMA_HISTOGRAM_BOOLEAN("GPU.SupportsVulkan", gpu_info->supports_vulkan);
  UMA_HISTOGRAM_ENUMERATION(
      "GPU.VulkanVersion",
      ConvertToHistogramVulkanVersion(gpu_info->vulkan_version));

  for (size_t i = 0; i < vulkan_extensions.size(); ++i) {
    std::string name = "GPU.VulkanExtSupport.";
    name.append(vulkan_extensions[i]);
    base::UmaHistogramBoolean(name, extension_support[i]);
  }
}

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectGraphicsInfo");

  DCHECK(gpu_info);

  if (!CollectGraphicsInfoGL(gpu_info))
    return false;

  // ANGLE's renderer strings are of the form:
  // ANGLE (<adapter_identifier> Direct3D<version> vs_x_x ps_x_x)
  std::string direct3d_version;
  int vertex_shader_major_version = 0;
  int vertex_shader_minor_version = 0;
  int pixel_shader_major_version = 0;
  int pixel_shader_minor_version = 0;
  if (RE2::FullMatch(gpu_info->gl_renderer,
                     "ANGLE \\(.*\\)") &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " Direct3D(\\w+)",
                        &direct3d_version) &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " vs_(\\d+)_(\\d+)",
                        &vertex_shader_major_version,
                        &vertex_shader_minor_version) &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " ps_(\\d+)_(\\d+)",
                        &pixel_shader_major_version,
                        &pixel_shader_minor_version)) {
    gpu_info->vertex_shader_version =
        base::StringPrintf("%d.%d",
                           vertex_shader_major_version,
                           vertex_shader_minor_version);
    gpu_info->pixel_shader_version =
        base::StringPrintf("%d.%d",
                           pixel_shader_major_version,
                           pixel_shader_minor_version);

    DCHECK(!gpu_info->vertex_shader_version.empty());
    // Note: do not reorder, used by UMA_HISTOGRAM below
    enum ShaderModel {
      SHADER_MODEL_UNKNOWN,
      SHADER_MODEL_2_0,
      SHADER_MODEL_3_0,
      SHADER_MODEL_4_0,
      SHADER_MODEL_4_1,
      SHADER_MODEL_5_0,
      NUM_SHADER_MODELS
    };
    ShaderModel shader_model = SHADER_MODEL_UNKNOWN;
    if (gpu_info->vertex_shader_version == "5.0") {
      shader_model = SHADER_MODEL_5_0;
    } else if (gpu_info->vertex_shader_version == "4.1") {
      shader_model = SHADER_MODEL_4_1;
    } else if (gpu_info->vertex_shader_version == "4.0") {
      shader_model = SHADER_MODEL_4_0;
    } else if (gpu_info->vertex_shader_version == "3.0") {
      shader_model = SHADER_MODEL_3_0;
    } else if (gpu_info->vertex_shader_version == "2.0") {
      shader_model = SHADER_MODEL_2_0;
    }
    UMA_HISTOGRAM_ENUMERATION("GPU.D3DShaderModel", shader_model,
                              NUM_SHADER_MODELS);

    // DirectX diagnostics are collected asynchronously because it takes a
    // couple of seconds.
  }
  return true;
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OFFICIAL_BUILD)
// This function has a real implementation for official builds that can
// be found in src/third_party/amd.
bool GetAMDSwitchableInfo(bool* is_switchable,
                          uint32_t* active_vendor_id,
                          uint32_t* active_device_id);
#else
bool GetAMDSwitchableInfo(bool* is_switchable,
                          uint32_t* active_vendor_id,
                          uint32_t* active_device_id) {
  return false;
}
#endif

namespace {

void CorrectAMDSwitchableInfo(angle::SystemInfo* system_info) {
  bool is_switchable = false;
  uint32_t active_vendor_id = 0;
  uint32_t active_device_id = 0;
  if (!GetAMDSwitchableInfo(&is_switchable, &active_vendor_id,
                            &active_device_id)) {
    return;
  }

  system_info->isAMDSwitchable = is_switchable;

  bool found_active = false;
  for (size_t i = 0; i < system_info->gpus.size(); ++i) {
    if (system_info->gpus[i].vendorId == active_vendor_id &&
        system_info->gpus[i].deviceId == active_device_id) {
      found_active = true;
      system_info->activeGPUIndex = static_cast<int>(i);
      break;
    }
  }
}

}  // anonymous namespace

bool CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectBasicGraphicsInfo");

  DCHECK(gpu_info);

  angle::SystemInfo system_info;
  bool collection_successful = angle::GetSystemInfo(&system_info);

  // We can get more precise information about AMD switchable systems in
  // official builds by querying the driver directly.
  if (system_info.isAMDSwitchable) {
    CorrectAMDSwitchableInfo(&system_info);
  }

  // Always fill in gpu_info so that information, even partial is displayed in
  // about:gpu
  FillGPUInfoFromSystemInfo(gpu_info, &system_info);

  if (collection_successful) {
    return true;
  }

  // angle::GetSystemInfo fails in remote sessions because EnumDisplayDevices
  // returns an empty id (unless that session happens to be attached to the
  // console). We don't want to fail in that case as we should still have a
  // device/vendor id. Only fail if the device string doesn't match the RDP
  // mirror driver or the citrix display driver.
  if (system_info.primaryDisplayDeviceId == "RDPUDD Chained DD" ||
      system_info.primaryDisplayDeviceId ==
          "Citrix Systems Inc. Display Driver") {
    return true;
  }

  // On failure set fake vendor_id/device_id for blacklisting purpose.
  gpu_info->gpu.vendor_id = 0xffff;
  gpu_info->gpu.device_id = 0xfffe;
  return false;
}

void CollectDriverInfoGL(GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectDriverInfoGL");

  if (!gpu_info->driver_version.empty())
    return;

  RE2::PartialMatch(gpu_info->gl_version, "([\\d\\.]+)$",
                    &gpu_info->driver_version);
}

}  // namespace gpu
