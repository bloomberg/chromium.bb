// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TESTAPP_GL_RENDERER_H_
#define CHROME_BROWSER_VR_TESTAPP_GL_RENDERER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/vr/base_compositor_delegate.h"
#include "ui/gfx/swap_result.h"

namespace gl {
class GLSurface;
}  // namespace gl

namespace vr {

class VrTestContext;

// This class manages an OpenGL context and initiates per-frame rendering.
class GlRenderer : public BaseCompositorDelegate {
 public:
  GlRenderer();
  ~GlRenderer() override;

  bool Initialize(const scoped_refptr<gl::GLSurface>& surface) override;
  void RenderFrame();
  void PostRenderFrameTask();
  void set_vr_context(VrTestContext* vr_context) { vr_context_ = vr_context; }

 private:
  VrTestContext* vr_context_;

  base::WeakPtrFactory<GlRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GlRenderer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TESTAPP_GL_RENDERER_H_
