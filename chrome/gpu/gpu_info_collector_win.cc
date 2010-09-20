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
#include "base/string_util.h"

// ANGLE seems to require that main.h be included before any other ANGLE header.
#include "libEGL/main.h"
#include "libEGL/Display.h"

namespace gpu_info_collector {

bool CollectGraphicsInfo(GPUInfo* gpu_info) {
  // TODO: collect OpenGL info if not using ANGLE?
  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2)
    return true;

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

  return CollectGraphicsInfoD3D(d3d, gpu_info);
}

bool CollectGraphicsInfoD3D(IDirect3D9* d3d, GPUInfo* gpu_info) {
  DCHECK(d3d);
  DCHECK(gpu_info);

  // Get device information
  D3DADAPTER_IDENTIFIER9 identifier;
  HRESULT hr = d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &identifier);
  if (hr != D3D_OK) {
    d3d->Release();
    return false;
  }
  uint32 vendor_id = identifier.VendorId;
  uint32 device_id = identifier.DeviceId;

  // Get version information
  D3DCAPS9 d3d_caps;
  HRESULT caps_result = d3d->GetDeviceCaps(D3DADAPTER_DEFAULT,
                                           D3DDEVTYPE_HAL,
                                           &d3d_caps);
  if (caps_result != D3D_OK) {
    d3d->Release();
    return false;
  }
  uint32 driver_major_version_hi = HIWORD(identifier.DriverVersion.HighPart);
  uint32 driver_major_version_lo = LOWORD(identifier.DriverVersion.HighPart);
  uint32 driver_minor_version_hi = HIWORD(identifier.DriverVersion.LowPart);
  uint32 driver_minor_version_lo = LOWORD(identifier.DriverVersion.LowPart);
  std::wstring driver_version = StringPrintf(L"%d.%d.%d.%d",
                                             driver_major_version_hi,
                                             driver_major_version_lo,
                                             driver_minor_version_hi,
                                             driver_minor_version_lo);

  bool can_lose_context = false;
  IDirect3D9Ex* d3dex = NULL;
  if (SUCCEEDED(d3d->QueryInterface(__uuidof(IDirect3D9Ex),
                                    reinterpret_cast<void**>(&d3dex)))) {
    d3dex->Release();
  } else {
    can_lose_context = true;
  }

  d3d->Release();

  // Get shader versions
  uint32 pixel_shader_version = d3d_caps.PixelShaderVersion;
  uint32 vertex_shader_version = d3d_caps.VertexShaderVersion;

  gpu_info->SetGraphicsInfo(vendor_id,
                           device_id,
                           driver_version,
                           pixel_shader_version,
                           vertex_shader_version,
                           0,
                           can_lose_context);
  return true;
}

bool CollectGraphicsInfoGL(GPUInfo* gpu_info) {
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
  if (id.empty()) {
    return false;
  }
  uint32 vendor_id;
  uint32 device_id;
  std::wstring vendorid = id.substr(8, 4);
  std::wstring deviceid = id.substr(17, 4);
  swscanf_s(vendorid.c_str(), L"%x", &vendor_id);
  swscanf_s(deviceid.c_str(), L"%x", &device_id);

  std::wstring driver_version = L"";
  uint32 pixel_shader_version = 0;
  uint32 vertex_shader_version = 0;
  gpu_info->SetGraphicsInfo(vendor_id, device_id, driver_version,
                            pixel_shader_version,
                            vertex_shader_version,
                            0,  // GL version of 0 indicates D3D
                            false);
  return true;

  // TODO(rlp): Add driver and pixel versions
}

}  // namespace gpu_info_collector
