// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_info_collector.h"

#include <windows.h>
#include <d3d9.h>

#include "app/gfx/gl/gl_context_egl.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/file_path.h"
#include "base/scoped_native_library.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

// ANGLE seems to require that main.h be included before any other ANGLE header.
#include "libEGL/main.h"
#include "libEGL/Display.h"

namespace gpu_info_collector {

bool CollectGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    gpu_info->SetLevel(GPUInfo::kComplete);
    return CollectGraphicsInfoGL(gpu_info);
  }

  // TODO(zmo): the following code only works if running on top of ANGLE.
  // Need to handle the case when running on top of real EGL/GLES2 drivers.

  egl::Display* display = static_cast<egl::Display*>(
      gfx::BaseEGLContext::GetDisplay());
  if (!display)
    return false;

  IDirect3DDevice9* device = display->getDevice();
  if (!device)
    return false;

  IDirect3D9* d3d = NULL;
  if (FAILED(device->GetDirect3D(&d3d)))
    return false;

  if (!CollectGraphicsInfoD3D(d3d, gpu_info))
    return false;

  // DirectX diagnostics are collected asynchronously because it takes a
  // couple of seconds. Do not mark as complete until that is done.
  gpu_info->SetLevel(GPUInfo::kPartial);
  return true;
}

bool CollectPreliminaryGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  gpu_info->SetLevel(GPUInfo::kPartial);

  bool rt = true;
  if (!CollectVideoCardInfo(gpu_info))
    rt = false;

  return rt;
}

bool CollectGraphicsInfoD3D(IDirect3D9* d3d, GPUInfo* gpu_info) {
  DCHECK(d3d);
  DCHECK(gpu_info);

  bool succeed = true;

  // Get device/driver information
  D3DADAPTER_IDENTIFIER9 identifier;
  if (d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &identifier) == D3D_OK) {
    gpu_info->SetVideoCardInfo(identifier.VendorId, identifier.DeviceId);

    uint32 driver_major_version_hi = HIWORD(identifier.DriverVersion.HighPart);
    uint32 driver_major_version_lo = LOWORD(identifier.DriverVersion.HighPart);
    uint32 driver_minor_version_hi = HIWORD(identifier.DriverVersion.LowPart);
    uint32 driver_minor_version_lo = LOWORD(identifier.DriverVersion.LowPart);
    std::string driver_version = StringPrintf("%d.%d.%d.%d",
                                              driver_major_version_hi,
                                              driver_major_version_lo,
                                              driver_minor_version_hi,
                                              driver_minor_version_lo);
    gpu_info->SetDriverInfo("", driver_version);
  } else {
    succeed = false;
  }

  // Get version information
  D3DCAPS9 d3d_caps;
  if (d3d->GetDeviceCaps(D3DADAPTER_DEFAULT,
                         D3DDEVTYPE_HAL,
                         &d3d_caps) == D3D_OK) {
    gpu_info->SetShaderVersion(d3d_caps.PixelShaderVersion,
                               d3d_caps.VertexShaderVersion);
  } else {
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
    return true;
  }
  return false;
}

bool CollectDriverInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  std::string gl_version_string = gpu_info->gl_version_string();

  // TODO(zmo): We assume the driver version is in the end of GL_VERSION
  // string.  Need to verify if it is true for majority drivers.

  size_t pos = gl_version_string.find_last_not_of("0123456789.");
  if (pos != std::string::npos && pos < gl_version_string.length() - 1) {
    gpu_info->SetDriverInfo("", gl_version_string.substr(pos + 1));
    return true;
  }
  return false;
}

}  // namespace gpu_info_collector
