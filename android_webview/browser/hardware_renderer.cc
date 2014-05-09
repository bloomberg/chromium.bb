// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/hardware_renderer.h"

#include "android_webview/browser/aw_gl_surface.h"
#include "android_webview/browser/browser_view_renderer_client.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/debug/trace_event.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"

namespace android_webview {

HardwareRenderer::HardwareRenderer(SharedRendererState* state)
    : shared_renderer_state_(state),
      last_egl_context_(eglGetCurrentContext()) {
  DCHECK(last_egl_context_);

  gl_surface_ = new AwGLSurface;
  bool success =
      shared_renderer_state_->GetCompositor()->
          InitializeHwDraw(gl_surface_);
  DCHECK(success);
}

HardwareRenderer::~HardwareRenderer() {
  shared_renderer_state_->GetCompositor()->ReleaseHwDraw();
  gl_surface_ = NULL;
}

bool HardwareRenderer::DrawGL(bool stencil_enabled,
                              int framebuffer_binding_ext,
                              AwDrawGLInfo* draw_info,
                              DrawGLResult* result) {
  TRACE_EVENT0("android_webview", "HardwareRenderer::DrawGL");

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

  if (draw_info->mode != AwDrawGLInfo::kModeDraw)
    return false;

  // Should only need to access SharedRendererState in kModeDraw and kModeSync.
  const DrawGLInput input = shared_renderer_state_->GetDrawGLInput();
  SetCompositorMemoryPolicy();

  gl_surface_->SetBackingFrameBufferObject(framebuffer_binding_ext);

  gfx::Transform transform;
  transform.matrix().setColMajorf(draw_info->transform);
  transform.Translate(input.scroll_offset.x(), input.scroll_offset.y());
  gfx::Rect clip_rect(draw_info->clip_left,
                      draw_info->clip_top,
                      draw_info->clip_right - draw_info->clip_left,
                      draw_info->clip_bottom - draw_info->clip_top);

  gfx::Rect viewport(draw_info->width, draw_info->height);
  if (!draw_info->is_layer) {
    gfx::RectF view_rect(input.width, input.height);
    transform.TransformRect(&view_rect);
    viewport.Intersect(gfx::ToEnclosingRect(view_rect));
  }

  bool did_draw = shared_renderer_state_->GetCompositor()->DemandDrawHw(
      gfx::Size(draw_info->width, draw_info->height),
      transform,
      viewport,
      clip_rect,
      stencil_enabled);
  gl_surface_->ResetBackingFrameBufferObject();

  if (did_draw) {
    result->frame_id = input.frame_id;
    result->clip_contains_visible_rect =
        clip_rect.Contains(input.global_visible_rect);
  }
  return did_draw;
}

void HardwareRenderer::SetCompositorMemoryPolicy() {
  if (shared_renderer_state_->IsMemoryPolicyDirty()) {
    content::SynchronousCompositorMemoryPolicy policy =
        shared_renderer_state_->GetMemoryPolicy();
    // Memory policy is set by BrowserViewRenderer on UI thread.
    shared_renderer_state_->GetCompositor()->SetMemoryPolicy(policy);
    shared_renderer_state_->SetMemoryPolicyDirty(false);
  }
}

}  // namespace android_webview
