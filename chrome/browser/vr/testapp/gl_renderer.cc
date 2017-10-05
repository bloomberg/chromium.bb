// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/testapp/gl_renderer.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/vr/testapp/vr_test_context.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace vr {

GlRenderer::GlRenderer(const scoped_refptr<gl::GLSurface>& surface,
                       const gfx::Size& size,
                       vr::VrTestContext* vr)
    : surface_(surface), size_(size), vr_(vr), weak_ptr_factory_(this) {}

GlRenderer::~GlRenderer() {}

bool GlRenderer::Initialize() {
  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create GL context";
    return false;
  }

  surface_->Resize(size_, 1.f, gl::GLSurface::ColorSpace::UNSPECIFIED, true);

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Failed to make GL context current";
    return false;
  }

  vr_->OnGlInitialized(size_);

  PostRenderFrameTask(gfx::SwapResult::SWAP_ACK);
  return true;
}

void GlRenderer::RenderFrame() {
  context_->MakeCurrent(surface_.get());
  vr_->DrawFrame();
  PostRenderFrameTask(surface_->SwapBuffers());
}

void GlRenderer::PostRenderFrameTask(gfx::SwapResult result) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GlRenderer::RenderFrame, weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSecondsD(1.0 / 60));
}

}  // namespace vr
