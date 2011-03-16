// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>

#include "base/message_loop.h"
#include "content/renderer/ggl.h"
#include "content/renderer/media/gles2_video_decode_context.h"

Gles2VideoDecodeContext::Gles2VideoDecodeContext(
    MessageLoop* message_loop, bool memory_mapped, ggl::Context* context)
    : message_loop_(message_loop),
      memory_mapped_(memory_mapped),
      context_(context) {
}

Gles2VideoDecodeContext::~Gles2VideoDecodeContext() {
}

void* Gles2VideoDecodeContext::GetDevice() {
  // This decode context is used inside the renderer and so hardware decoder
  // device handler should not be used.
  return NULL;
}

void Gles2VideoDecodeContext::AllocateVideoFrames(
    int num_frames, size_t width, size_t height,
    media::VideoFrame::Format format,
    std::vector<scoped_refptr<media::VideoFrame> >* frames_out, Task* task) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &Gles2VideoDecodeContext::AllocateVideoFrames,
                          num_frames, width, height, format, frames_out,
                          task));
    return;
  }

  // In this method we need to make the ggl context current and then generate
  // textures for each video frame. We also need to allocate memory for each
  // texture generated.
  bool ret = ggl::MakeCurrent(context_);
  CHECK(ret) << "Failed to switch context";

  frames_.resize(num_frames);
  for (int i = 0; i < num_frames; ++i) {
    int planes = media::VideoFrame::GetNumberOfPlanes(format);
    media::VideoFrame::GlTexture textures[media::VideoFrame::kMaxPlanes];

    // Set the color format of the textures.
    DCHECK(format == media::VideoFrame::RGBA ||
           format == media::VideoFrame::YV12);
    int gl_format = format == media::VideoFrame::RGBA ? GL_RGBA : GL_LUMINANCE;

    glGenTextures(planes, textures);
    for (int j = 0; j < planes; ++j) {
      glBindTexture(GL_TEXTURE_2D, textures[j]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, gl_format, width, height, 0, gl_format,
                   GL_UNSIGNED_BYTE, NULL);
    }
    glFlush();

    scoped_refptr<media::VideoFrame> frame;
    media::VideoFrame::CreateFrameGlTexture(format, width, height, textures,
                                            &frame);
    frames_[i] = frame;
  }
  *frames_out = frames_;

  task->Run();
  delete task;
}

void Gles2VideoDecodeContext::ReleaseAllVideoFrames() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &Gles2VideoDecodeContext::ReleaseAllVideoFrames));
    return;
  }

  // Make the context current and then release the video frames.
  bool ret = ggl::MakeCurrent(context_);
  CHECK(ret) << "Failed to switch context";

  for (size_t i = 0; i < frames_.size(); ++i) {
    for (size_t j = 0; j < frames_[i]->planes(); ++j) {
      media::VideoFrame::GlTexture texture = frames_[i]->gl_texture(j);
      glDeleteTextures(1, &texture);
    }
  }
  frames_.clear();
}

void Gles2VideoDecodeContext::ConvertToVideoFrame(
    void* buffer, scoped_refptr<media::VideoFrame> frame, Task* task) {
  DCHECK(memory_mapped_);
  // TODO(hclam): Implement.
}

void Gles2VideoDecodeContext::Destroy(Task* task) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &Gles2VideoDecodeContext::Destroy, task));
    return;
  }

  ReleaseAllVideoFrames();
  DCHECK_EQ(0u, frames_.size());

  task->Run();
  delete task;
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(Gles2VideoDecodeContext);
