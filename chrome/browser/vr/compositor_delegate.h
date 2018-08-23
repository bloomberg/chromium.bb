// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_COMPOSITOR_DELEGATE_H_
#define CHROME_BROWSER_VR_COMPOSITOR_DELEGATE_H_

#include "base/callback.h"
#include "chrome/browser/vr/compositor_ui_interface.h"
#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace gfx {
class Transform;
}

namespace gl {
class GLSurface;
}  // namespace gl

namespace vr {

struct RenderInfo;

class VR_EXPORT CompositorDelegate {
 public:
  using Transform = float[16];
  enum FrameType { kUiFrame, kWebXrFrame };
  using SkiaContextCallback = base::OnceCallback<void()>;
  virtual ~CompositorDelegate() {}

  virtual FovRectangles GetRecommendedFovs() = 0;
  virtual float GetZNear() = 0;
  virtual RenderInfo GetRenderInfo(FrameType frame_type) = 0;
  virtual RenderInfo GetOptimizedRenderInfoForFovs(
      const FovRectangles& fovs) = 0;
  virtual void InitializeBuffers() = 0;
  virtual void PrepareBufferForWebXr() = 0;
  virtual void PrepareBufferForWebXrOverlayElements() = 0;
  virtual void PrepareBufferForContentQuadLayer(
      const gfx::Transform& quad_transform) = 0;
  virtual void PrepareBufferForBrowserUi() = 0;
  virtual void OnFinishedDrawingBuffer() = 0;
  virtual void GetWebXrDrawParams(int* texture_id, Transform* uv_transform) = 0;
  virtual bool IsContentQuadReady() = 0;
  virtual void ResumeContentRendering() = 0;
  virtual void BufferBoundsChanged(const gfx::Size& content_buffer_size,
                                   const gfx::Size& overlay_buffer_size) = 0;
  virtual void GetContentQuadDrawParams(Transform* uv_transform,
                                        float* border_x,
                                        float* border_y) = 0;
  virtual void SubmitFrame(FrameType frame_type) = 0;

  virtual void SetUiInterface(CompositorUiInterface* ui) = 0;
  virtual void SetShowingVrDialog(bool showing) = 0;
  virtual int GetContentBufferWidth() = 0;

  virtual void ConnectPresentingService(
      device::mojom::VRDisplayInfoPtr display_info,
      device::mojom::XRRuntimeSessionOptionsPtr options) = 0;

  // These methods return true when succeeded.
  virtual bool Initialize(const scoped_refptr<gl::GLSurface>& surface) = 0;
  virtual bool RunInSkiaContext(SkiaContextCallback callback) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_COMPOSITOR_DELEGATE_H_
