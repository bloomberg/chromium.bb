// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_info_collector.h"

#include <d3d9.h>
#include <setupapi.h>
#include <windows.h>

#include "app/gfx/gl/gl_context_egl.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

// ANGLE seems to require that main.h be included before any other ANGLE header.
#include "libEGL/main.h"
#include "libEGL/Display.h"

// Setup API functions
typedef HDEVINFO (WINAPI*SetupDiGetClassDevsWFunc)(
    CONST GUID *ClassGuid,
    PCWSTR Enumerator,
    HWND hwndParent,
    DWORD Flags
);
typedef BOOL (WINAPI*SetupDiEnumDeviceInfoFunc)(
    HDEVINFO DeviceInfoSet,
    DWORD MemberIndex,
    PSP_DEVINFO_DATA DeviceInfoData
);
typedef BOOL (WINAPI*SetupDiGetDeviceRegistryPropertyWFunc)(
    HDEVINFO DeviceInfoSet,
    PSP_DEVINFO_DATA DeviceInfoData,
    DWORD Property,
    PDWORD PropertyRegDataType,
    PBYTE PropertyBuffer,
    DWORD PropertyBufferSize,
    PDWORD RequiredSize
);
typedef BOOL (WINAPI*SetupDiDestroyDeviceInfoListFunc)(
    HDEVINFO DeviceInfoSet
);

namespace gpu_info_collector {

bool CollectGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    gpu_info->SetLevel(GPUInfo::kComplete);
    return CollectGraphicsInfoGL(gpu_info);
  }

  // Set to partial now in case this function returns false below.
  gpu_info->SetLevel(GPUInfo::kPartial);

  // TODO(zmo): the following code only works if running on top of ANGLE.
  // Need to handle the case when running on top of real EGL/GLES2 drivers.

  egl::Display* display = static_cast<egl::Display*>(
      gfx::BaseEGLContext::GetDisplay());
  if (!display) {
    LOG(ERROR) << "gfx::BaseEGLContext::GetDisplay() failed";
    return false;
  }

  IDirect3DDevice9* device = display->getDevice();
  if (!device) {
    LOG(ERROR) << "display->getDevice() failed";
    return false;
  }

  IDirect3D9* d3d = NULL;
  if (FAILED(device->GetDirect3D(&d3d))) {
    LOG(ERROR) << "device->GetDirect3D(&d3d) failed";
    return false;
  }

  if (!CollectGraphicsInfoD3D(d3d, gpu_info))
    return false;

  // DirectX diagnostics are collected asynchronously because it takes a
  // couple of seconds. Do not mark gpu_info as complete until that is done.
  return true;
}

bool CollectPreliminaryGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  gpu_info->SetLevel(GPUInfo::kPreliminary);

  bool rt = true;
  if (!CollectVideoCardInfo(gpu_info))
    rt = false;

  return rt;
}

bool CollectGraphicsInfoD3D(IDirect3D9* d3d, GPUInfo* gpu_info) {
  DCHECK(d3d);
  DCHECK(gpu_info);

  bool succeed = CollectVideoCardInfo(gpu_info);

  // Get version information
  D3DCAPS9 d3d_caps;
  if (d3d->GetDeviceCaps(D3DADAPTER_DEFAULT,
                         D3DDEVTYPE_HAL,
                         &d3d_caps) == D3D_OK) {
    gpu_info->SetShaderVersion(d3d_caps.PixelShaderVersion,
                               d3d_caps.VertexShaderVersion);
  } else {
    LOG(ERROR) << "d3d->GetDeviceCaps() failed";
    succeed = false;
  }

  // Get can_lose_context
  bool can_lose_context = false;
  IDirect3D9Ex* d3dex = NULL;
  if (SUCCEEDED(d3d->QueryInterface(__uuidof(IDirect3D9Ex),
                                    reinterpret_cast<void**>(&d3dex)))) {
    d3dex->Release();
  } else {
    can_lose_context = true;
  }
  gpu_info->SetCanLoseContext(can_lose_context);

  d3d->Release();
  return true;
}

bool CollectVideoCardInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  // Taken from http://developer.nvidia.com/object/device_ids.html
  DISPLAY_DEVICE dd;
  dd.cb = sizeof(DISPLAY_DEVICE);
  int i = 0;
  std::wstring id;
  for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); ++i) {
    if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      id = dd.DeviceID;
      break;
    }
  }

  if (id.length() > 20) {
    int vendor_id = 0, device_id = 0;
    std::wstring vendor_id_string = id.substr(8, 4);
    std::wstring device_id_string = id.substr(17, 4);
    base::HexStringToInt(WideToASCII(vendor_id_string), &vendor_id);
    base::HexStringToInt(WideToASCII(device_id_string), &device_id);
    gpu_info->SetVideoCardInfo(vendor_id, device_id);
    // TODO(zmo): we only need to call CollectDriverInfoD3D() if we use ANGLE.
    return CollectDriverInfoD3D(id, gpu_info);
  }
  return false;
}

bool CollectDriverInfoD3D(const std::wstring& device_id, GPUInfo* gpu_info) {
  HMODULE lib_setupapi = LoadLibraryW(L"setupapi.dll");
  if (!lib_setupapi) {
    LOG(ERROR) << "Open setupapi.dll failed";
    return false;
  }
  SetupDiGetClassDevsWFunc fp_get_class_devs =
      reinterpret_cast<SetupDiGetClassDevsWFunc>(
          GetProcAddress(lib_setupapi, "SetupDiGetClassDevsW"));
  SetupDiEnumDeviceInfoFunc fp_enum_device_info =
      reinterpret_cast<SetupDiEnumDeviceInfoFunc>(
          GetProcAddress(lib_setupapi, "SetupDiEnumDeviceInfo"));
  SetupDiGetDeviceRegistryPropertyWFunc fp_get_device_registry_property =
      reinterpret_cast<SetupDiGetDeviceRegistryPropertyWFunc>(
          GetProcAddress(lib_setupapi, "SetupDiGetDeviceRegistryPropertyW"));
  SetupDiDestroyDeviceInfoListFunc fp_destroy_device_info_list =
      reinterpret_cast<SetupDiDestroyDeviceInfoListFunc>(
          GetProcAddress(lib_setupapi, "SetupDiDestroyDeviceInfoList"));
  if (!fp_get_class_devs || !fp_enum_device_info ||
      !fp_get_device_registry_property || !fp_destroy_device_info_list) {
    FreeLibrary(lib_setupapi);
    LOG(ERROR) << "Retrieve setupapi.dll functions failed";
    return false;
  }

  // create device info for the display device
  HDEVINFO device_info = fp_get_class_devs(
    NULL, device_id.c_str(), NULL,
    DIGCF_PRESENT | DIGCF_PROFILE | DIGCF_ALLCLASSES);
  if (device_info == INVALID_HANDLE_VALUE) {
    FreeLibrary(lib_setupapi);
    LOG(ERROR) << "Creating device info failed";
    return false;
  }

  DWORD index = 0;
  bool found = false;
  SP_DEVINFO_DATA device_info_data;
  device_info_data.cbSize = sizeof(device_info_data);
  while (fp_enum_device_info(device_info, index++, &device_info_data)) {
    WCHAR value[255];
    if (fp_get_device_registry_property(device_info,
                                        &device_info_data,
                                        SPDRP_DRIVER,
                                        NULL,
                                        reinterpret_cast<PBYTE>(value),
                                        sizeof(value),
                                        NULL)) {
      HKEY key;
      std::wstring driver_key = L"System\\CurrentControlSet\\Control\\Class\\";
      driver_key += value;
      LONG result = RegOpenKeyExW(
          HKEY_LOCAL_MACHINE, driver_key.c_str(), 0, KEY_QUERY_VALUE, &key);
      if (result == ERROR_SUCCESS) {
        DWORD dwcb_data = sizeof(value);
        std::string driver_version;
        result = RegQueryValueExW(
            key, L"DriverVersion", NULL, NULL,
            reinterpret_cast<LPBYTE>(value), &dwcb_data);
        if (result == ERROR_SUCCESS)
          driver_version = WideToASCII(std::wstring(value));

        std::string driver_date;
        dwcb_data = sizeof(value);
        result = RegQueryValueExW(
            key, L"DriverDate", NULL, NULL,
            reinterpret_cast<LPBYTE>(value), &dwcb_data);
        if (result == ERROR_SUCCESS)
          driver_date = WideToASCII(std::wstring(value));

        gpu_info->SetDriverInfo("", driver_version, driver_date);
        found = true;
        RegCloseKey(key);
        break;
      }
    }
  }
  fp_destroy_device_info_list(device_info);
  FreeLibrary(lib_setupapi);
  return found;
}

bool CollectDriverInfoGL(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  std::string gl_version_string = gpu_info->gl_version_string();

  // TODO(zmo): We assume the driver version is in the end of GL_VERSION
  // string.  Need to verify if it is true for majority drivers.

  size_t pos = gl_version_string.find_last_not_of("0123456789.");
  if (pos != std::string::npos && pos < gl_version_string.length() - 1) {
    gpu_info->SetDriverInfo("", gl_version_string.substr(pos + 1), "");
    return true;
  }
  return false;
}

}  // namespace gpu_info_collector
