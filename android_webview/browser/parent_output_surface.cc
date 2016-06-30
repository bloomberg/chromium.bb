// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/parent_output_surface.h"

#include "android_webview/browser/aw_render_thread_context_provider.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface_client.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace android_webview {

ParentOutputSurface::ParentOutputSurface(
    scoped_refptr<AwRenderThreadContextProvider> context_provider)
    : cc::OutputSurface(std::move(context_provider), nullptr, nullptr) {
  stencil_state_.stencil_test_enabled = false;
}

ParentOutputSurface::~ParentOutputSurface() {
}

void ParentOutputSurface::DidLoseOutputSurface() {
  // Android WebView does not handle context loss.
  LOG(FATAL) << "Render thread context loss";
}

void ParentOutputSurface::Reshape(const gfx::Size& size,
                                  float scale_factor,
                                  const gfx::ColorSpace& color_space,
                                  bool has_alpha) {
  DCHECK_EQ(1.f, scale_factor);
  surface_size_ = size;
}

void ParentOutputSurface::SwapBuffers(cc::CompositorFrame frame) {
  context_provider_->ContextGL()->ShallowFlushCHROMIUM();
  client_->DidSwapBuffers();
}

void ParentOutputSurface::ApplyExternalStencil() {
  DCHECK(stencil_state_.stencil_test_enabled);
  gpu::gles2::GLES2Interface* gl = context_provider()->ContextGL();
  gl->StencilFuncSeparate(GL_FRONT, stencil_state_.stencil_front_func,
                          stencil_state_.stencil_front_mask,
                          stencil_state_.stencil_front_ref);
  gl->StencilFuncSeparate(GL_BACK, stencil_state_.stencil_back_func,
                          stencil_state_.stencil_back_mask,
                          stencil_state_.stencil_back_ref);
  gl->StencilMaskSeparate(GL_FRONT, stencil_state_.stencil_front_writemask);
  gl->StencilMaskSeparate(GL_BACK, stencil_state_.stencil_back_writemask);
  gl->StencilOpSeparate(GL_FRONT, stencil_state_.stencil_front_fail_op,
                        stencil_state_.stencil_front_z_fail_op,
                        stencil_state_.stencil_front_z_pass_op);
  gl->StencilOpSeparate(GL_BACK, stencil_state_.stencil_back_fail_op,
                        stencil_state_.stencil_back_z_fail_op,
                        stencil_state_.stencil_back_z_pass_op);
}

uint32_t ParentOutputSurface::GetFramebufferCopyTextureFormat() {
  auto* gl = static_cast<AwRenderThreadContextProvider*>(context_provider());
  return gl->GetCopyTextureInternalFormat();
}

void ParentOutputSurface::SetGLState(const ScopedAppGLStateRestore& gl_state) {
  stencil_state_ = gl_state.stencil_state();
  SetExternalStencilTest(stencil_state_.stencil_test_enabled);
}

}  // namespace android_webview
