// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mf/d3d_util.h"

#include <d3d9.h>
#include <dxva2api.h>

#include "base/scoped_comptr_win.h"

namespace media {

IDirect3DDeviceManager9* CreateD3DDevManager(HWND video_window,
                                             IDirect3D9** direct3d,
                                             IDirect3DDevice9** device) {
  ScopedComPtr<IDirect3DDeviceManager9> dev_manager;
  ScopedComPtr<IDirect3D9> d3d;
  d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
  if (d3d == NULL) {
    LOG(ERROR) << "Failed to create D3D9";
    return NULL;
  }
  D3DPRESENT_PARAMETERS present_params = {0};

  // Once we know the dimensions, we need to reset using
  // AdjustD3DDeviceBackBufferDimensions().
  present_params.BackBufferWidth = 0;
  present_params.BackBufferHeight = 0;
  present_params.BackBufferFormat = D3DFMT_UNKNOWN;
  present_params.BackBufferCount = 1;
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = video_window;
  present_params.Windowed = TRUE;
  present_params.Flags = D3DPRESENTFLAG_VIDEO;
  present_params.FullScreen_RefreshRateInHz = 0;
  present_params.PresentationInterval = 0;

  ScopedComPtr<IDirect3DDevice9> temp_device;

  // D3DCREATE_HARDWARE_VERTEXPROCESSING specifies hardware vertex processing.
  // (Is it even needed for just video decoding?)
  HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT,
                                 D3DDEVTYPE_HAL,
                                 NULL,
                                 D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                 &present_params,
                                 temp_device.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create D3D Device";
    return NULL;
  }
  UINT dev_manager_reset_token = 0;
  hr = DXVA2CreateDirect3DDeviceManager9(&dev_manager_reset_token,
                                         dev_manager.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Couldn't create D3D Device manager";
    return NULL;
  }
  hr = dev_manager->ResetDevice(temp_device.get(), dev_manager_reset_token);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set device to device manager";
    return NULL;
  }
  *direct3d = d3d.Detach();
  *device = temp_device.Detach();
  return dev_manager.Detach();
}

bool AdjustD3DDeviceBackBufferDimensions(IDirect3DDevice9* device,
                                         HWND video_window,
                                         int width,
                                         int height) {
  D3DPRESENT_PARAMETERS present_params = {0};
  present_params.BackBufferWidth = width;
  present_params.BackBufferHeight = height;
  present_params.BackBufferFormat = D3DFMT_UNKNOWN;
  present_params.BackBufferCount = 1;
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = video_window;
  present_params.Windowed = TRUE;
  present_params.Flags = D3DPRESENTFLAG_VIDEO;
  present_params.FullScreen_RefreshRateInHz = 0;
  present_params.PresentationInterval = 0;

  return SUCCEEDED(device->Reset(&present_params)) ? true : false;
}

}  // namespace media

