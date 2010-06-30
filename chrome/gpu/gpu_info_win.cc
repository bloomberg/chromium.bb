// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_info.h"

#include <windows.h>
#include <d3d9.h>

#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/string_util.h"

bool GPUInfo::CollectGraphicsInfo() {
  FilePath d3d_path(base::GetNativeLibraryName(L"d3d9"));
  base::ScopedNativeLibrary d3dlib(d3d_path);

  typedef IDirect3D9* (WINAPI *Direct3DCreate9Proc)(UINT);
  Direct3DCreate9Proc d3d_create_proc =
      static_cast<Direct3DCreate9Proc>(
      d3dlib.GetFunctionPointer("Direct3DCreate9"));

  if (!d3d_create_proc) {
    return false;
  }
  IDirect3D9 *d3d = d3d_create_proc(D3D_SDK_VERSION);
  if (!d3d) {
    return false;
  }
  return CollectGraphicsInfoD3D(d3d);
}

bool GPUInfo::CollectGraphicsInfoD3D(IDirect3D9* d3d) {
  // Get device information
  D3DADAPTER_IDENTIFIER9 identifier;
  HRESULT hr = d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &identifier);
  if (hr != D3D_OK) {
    d3d->Release();
    return false;
  }
  vendor_id_ = identifier.VendorId;
  device_id_ = identifier.DeviceId;

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
  driver_version_ = StringPrintf(L"%d.%d.%d.%d",
                                 driver_major_version_hi,
                                 driver_major_version_lo,
                                 driver_minor_version_hi,
                                 driver_minor_version_lo);
  d3d->Release();

  // Get shader versions
  pixel_shader_version_ = d3d_caps.PixelShaderVersion;
  vertex_shader_version_ = d3d_caps.VertexShaderVersion;
  return true;
}

bool GPUInfo::CollectGraphicsInfoGL() {
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
  std::wstring vendorid = id.substr(8, 4);
  std::wstring deviceid = id.substr(17, 4);
  swscanf_s(vendorid.c_str(), L"%x", &vendor_id_);
  swscanf_s(deviceid.c_str(), L"%x", &device_id_);
  return true;

  // TODO(rlp): Add driver and pixel versions
}
