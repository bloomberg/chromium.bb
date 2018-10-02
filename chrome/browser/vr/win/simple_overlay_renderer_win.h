// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_SIMPLE_OVERLAY_RENDERER_WIN_H_
#define CHROME_BROWSER_VR_WIN_SIMPLE_OVERLAY_RENDERER_WIN_H_

#include <d3d11.h>
#include <wrl.h>

#include "mojo/public/cpp/system/handle.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

// This class renders an simple solid-color overlay that can be submitted to
// be composited on top of WebXR content.  Note that it is not used outside
// manual testing (requires build changes to enable), and will be replaced with
// VR-UI overlays rendered through the command buffer.
class SimpleOverlayRenderer {
 public:
  SimpleOverlayRenderer();
  ~SimpleOverlayRenderer();

  void Initialize();
  void Render();

  mojo::ScopedHandle GetTexture();
  gfx::RectF GetLeft();
  gfx::RectF GetRight();

 private:
  // DirectX state.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
  Microsoft::WRL::ComPtr<ID3D11Device> device_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_SIMPLE_OVERLAY_RENDERER_WIN_H_
