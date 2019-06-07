// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_COM_DEFS_H_
#define MEDIA_GPU_WINDOWS_D3D11_COM_DEFS_H_

#include <d3d11.h>
#include <wrl/client.h>

// TODO(tmathmeyer) Ensure that Microsoft::WRL::ComPtr doesn't show up
// in any of the other files in media/gpu/windows.
namespace media {

// We want to shorten this so the |using| statements are single line -
// this improves readability greatly.
#define COM Microsoft::WRL::ComPtr

// Keep these sorted alphabetically.
using ComD3D11DeviceContext = COM<ID3D11DeviceContext>;
using ComD3D11Texture2D = COM<ID3D11Texture2D>;
using ComD3D11VideoContext = COM<ID3D11VideoContext>;
using ComD3D11VideoDecoderOutputView = COM<ID3D11VideoDecoderOutputView>;
using ComD3D11VideoDevice = COM<ID3D11VideoDevice>;
using ComD3D11VideoProcessor = COM<ID3D11VideoProcessor>;
using ComD3D11VideoProcessorEnumerator = COM<ID3D11VideoProcessorEnumerator>;

#undef COM

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_COM_DEFS_H_
