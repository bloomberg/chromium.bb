// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/hardware_renderer_legacy.h"

#include "android_webview/browser/aw_gl_surface.h"
#include "android_webview/browser/shared_renderer_state.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/debug/trace_event.h"
#include "base/strings/string_number_conversions.h"
#include "cc/output/compositor_frame.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"

namespace android_webview {

HardwareRendererLegacy::HardwareRendererLegacy(SharedRendererState* state)
    : shared_renderer_state_(state), last_egl_context_(eglGetCurrentContext()) {
  DCHECK(last_egl_context_);

  gl_surface_ = new AwGLSurface;
  bool success =
      shared_renderer_state_->GetCompositor()->InitializeHwDraw(gl_surface_);
  DCHECK(success);
}

HardwareRendererLegacy::~HardwareRendererLegacy() {
  draw_gl_input_ = shared_renderer_state_->PassDrawGLInput();
  shared_renderer_state_->GetCompositor()->ReleaseHwDraw();
  gl_surface_ = NULL;
}

bool HardwareRendererLegacy::DrawGL(bool stencil_enabled,
                                    int framebuffer_binding_ext,
                                    AwDrawGLInfo* draw_info,
                                    DrawGLResult* result) {
  TRACE_EVENT0("android_webview", "HardwareRendererLegacy::DrawGL");

  // We need to watch if the current Android context has changed and enforce
  // a clean-up in the compositor.
  EGLContext current_context = eglGetCurrentContext();
  if (!current_context) {
    DLOG(ERROR) << "DrawGL called without EGLContext";
    return false;
  }

  // TODO(boliu): Handle context loss.
  if (last_egl_context_ != current_context)
    DLOG(WARNING) << "EGLContextChanged";

  // Should only need to access SharedRendererState in kModeDraw and kModeSync.
  scoped_ptr<DrawGLInput> input = shared_renderer_state_->PassDrawGLInput();
  if (input.get())
    draw_gl_input_ = input.Pass();
  SetCompositorMemoryPolicy();

  gl_surface_->SetBackingFrameBufferObject(framebuffer_binding_ext);

  gfx::Transform transform;
  transform.matrix().setColMajorf(draw_info->transform);
  transform.Translate(draw_gl_input_->scroll_offset.x(),
                      draw_gl_input_->scroll_offset.y());
  gfx::Rect clip_rect(draw_info->clip_left,
                      draw_info->clip_top,
                      draw_info->clip_right - draw_info->clip_left,
                      draw_info->clip_bottom - draw_info->clip_top);

  gfx::Rect viewport(draw_info->width, draw_info->height);
  if (!draw_info->is_layer) {
    gfx::RectF view_rect(draw_gl_input_->width, draw_gl_input_->height);
    transform.TransformRect(&view_rect);
    viewport.Intersect(gfx::ToEnclosingRect(view_rect));
  }

  scoped_ptr<cc::CompositorFrame> frame =
      shared_renderer_state_->GetCompositor()->DemandDrawHw(
          gfx::Size(draw_info->width, draw_info->height),
          transform,
          viewport,
          clip_rect,
          framebuffer_binding_ext);
  gl_surface_->ResetBackingFrameBufferObject();

  if (frame.get()) {
    result->clip_contains_visible_rect =
        clip_rect.Contains(draw_gl_input_->global_visible_rect);
  }
  return !!frame.get();
}

void HardwareRendererLegacy::SetCompositorMemoryPolicy() {
  if (shared_renderer_state_->IsMemoryPolicyDirty()) {
    content::SynchronousCompositorMemoryPolicy policy =
        shared_renderer_state_->GetMemoryPolicy();
    // Memory policy is set by BrowserViewRenderer on UI thread.
    shared_renderer_state_->GetCompositor()->SetMemoryPolicy(policy);
    shared_renderer_state_->SetMemoryPolicyDirty(false);
  }
}

}  // namespace android_webview
