// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/gles2_video_decode_context.h"

Gles2VideoDecodeContext::Gles2VideoDecodeContext(
    StorageType type, ggl::Context* context)
    : message_loop_(MessageLoop::current()), type_(type), context_(context) {
}

Gles2VideoDecodeContext::~Gles2VideoDecodeContext() {
  // TODO(hclam): Implement.
}

void* Gles2VideoDecodeContext::GetDevice() {
  // This decode context is used inside the renderer and so hardware decoder
  // device handler should not be used.
  return NULL;
}

void Gles2VideoDecodeContext::AllocateVideoFrames(
    int n, size_t width, size_t height, media::VideoFrame::Format format,
    std::vector<scoped_refptr<media::VideoFrame> >* frames, Task* task) {
  // TODO(hclam): Implement.
}

void Gles2VideoDecodeContext::ReleaseAllVideoFrames() {
  // TODO(hclam): Implement.
}

void Gles2VideoDecodeContext::UploadToVideoFrame(
    void* buffer, scoped_refptr<media::VideoFrame> frame, Task* task) {
  // TODO(hclam): Implement.
}

void Gles2VideoDecodeContext::Destroy(Task* task) {
  // TODO(hclam): Implement.
}
