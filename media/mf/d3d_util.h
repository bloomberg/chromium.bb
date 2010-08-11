// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions for Direct3D Devices.

#ifndef MEDIA_MF_D3D_UTIL_H_
#define MEDIA_MF_D3D_UTIL_H_

#include <windows.h>

struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DDeviceManager9;

namespace media {

// Creates a Direct3D device manager for the given window.
IDirect3DDeviceManager9* CreateD3DDevManager(HWND video_window,
                                             IDirect3D9** direct3d,
                                             IDirect3DDevice9** device);

// Resets the D3D device to prevent scaling from happening because it was
// created with window before resizing occurred. We need to change the back
// buffer dimensions to the actual video frame dimensions.
// Both the decoder and device should be initialized before calling this method.
// Returns: true if successful.
bool AdjustD3DDeviceBackBufferDimensions(IDirect3DDevice9* device,
                                         HWND video_window,
                                         int width,
                                         int height);

}  // namespace media
#endif  // MEDIA_MF_D3D_UTIL_H_
