// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/frame_renderer_dummy.h"

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace media {
namespace test {

FrameRendererDummy::FrameRendererDummy() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FrameRendererDummy::~FrameRendererDummy() {}

// static
std::unique_ptr<FrameRendererDummy> FrameRendererDummy::Create() {
  auto frame_renderer = base::WrapUnique(new FrameRendererDummy());
  if (!frame_renderer->Initialize()) {
    return nullptr;
  }
  return frame_renderer;
}

bool FrameRendererDummy::Initialize() {
  return true;
}

void FrameRendererDummy::AcquireGLContext() {
  // As no actual rendering is done we don't have a GLContext to acquire.
}

void FrameRendererDummy::ReleaseGLContext() {
  // As no actual rendering is done we don't have a GLContext to release.
}

gl::GLContext* FrameRendererDummy::GetGLContext() {
  // As no actual rendering is done we don't have a GLContext.
  return nullptr;
}

void FrameRendererDummy::RenderFrame(scoped_refptr<VideoFrame> video_frame) {}

scoped_refptr<VideoFrame> FrameRendererDummy::CreateVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& size,
    uint32_t texture_target,
    uint32_t* texture_id) {
  return nullptr;
}

}  // namespace test
}  // namespace media
