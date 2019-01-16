// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/frame_renderer_dummy.h"

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace media {
namespace test {

FrameRendererDummy::FrameRendererDummy() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FrameRendererDummy::~FrameRendererDummy() {
  Destroy();
}

// static
std::unique_ptr<FrameRendererDummy> FrameRendererDummy::Create() {
  auto frame_renderer = base::WrapUnique(new FrameRendererDummy());
  if (!frame_renderer->Initialize()) {
    return nullptr;
  }
  return frame_renderer;
}

bool FrameRendererDummy::Initialize() {
#if defined(USE_OZONE)
  // Initialize Ozone. This is necessary even though we are not doing any actual
  // rendering. If not initialized a crash will occur when assigning picture
  // buffers, even when passing 0 as texture ID.
  // TODO(@dstaessens):
  // * Get rid of the Ozone dependency, as it forces us to call 'stop ui' when
  //   running tests.
  LOG(INFO) << "Initializing Ozone Platform...\n"
               "If this hangs indefinitely please call 'stop ui' first!";
  ui::OzonePlatform::InitParams params = {.single_process = false};
  ui::OzonePlatform::InitializeForUI(params);
  ui::OzonePlatform::InitializeForGPU(params);
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();

  // Initialize the Ozone GPU helper. If this is not done an error will occur:
  // "Check failed: drm. No devices available for buffer allocation."
  // Note: If a task environment is not set up initialization will hang
  // indefinitely here.
  gpu_helper_.reset(new ui::OzoneGpuTestHelper());
  gpu_helper_->Initialize(base::ThreadTaskRunnerHandle::Get());
#endif

  return true;
}

void FrameRendererDummy::Destroy() {
#if defined(USE_OZONE)
  gpu_helper_.reset();
#endif
}

void FrameRendererDummy::AcquireGLContext() {
  // As no actual rendering is done we don't have a GLContext to acquire.
}

void FrameRendererDummy::ReleaseGLContext() {
  // As no actual rendering is done we don't have a GLContext to release.
}

gl::GLContext* FrameRendererDummy::GetGLContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // As no actual rendering is done we don't have a GLContext.
  return nullptr;
}

void FrameRendererDummy::RenderFrame(scoped_refptr<VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace test
}  // namespace media
