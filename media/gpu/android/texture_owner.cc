// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/texture_owner.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

// FrameAvailableEvent is a RefCounted wrapper for a WaitableEvent
// (it's not possible to put one in RefCountedData).
// This lets us safely signal an event on any thread.
struct FrameAvailableEvent
    : public base::RefCountedThreadSafe<FrameAvailableEvent> {
  FrameAvailableEvent()
      : event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  void Signal() { event.Signal(); }
  base::WaitableEvent event;

 private:
  friend class RefCountedThreadSafe<FrameAvailableEvent>;
  ~FrameAvailableEvent() = default;
};

TextureOwner::TextureOwner()
    : base::RefCountedDeleteOnSequence<TextureOwner>(
          base::ThreadTaskRunnerHandle::Get()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

TextureOwner::~TextureOwner() = default;

scoped_refptr<TextureOwner> SurfaceTextureGLOwner::Create() {
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  if (!texture_id)
    return nullptr;

  // Set the parameters on the texture.
  gl::ScopedActiveTexture active_texture(GL_TEXTURE0);
  gl::ScopedTextureBinder texture_binder(GL_TEXTURE_EXTERNAL_OES, texture_id);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  return new SurfaceTextureGLOwner(texture_id);
}

SurfaceTextureGLOwner::SurfaceTextureGLOwner(GLuint texture_id)
    : surface_texture_(gl::SurfaceTexture::Create(texture_id)),
      texture_id_(texture_id),
      context_(gl::GLContext::GetCurrent()),
      surface_(gl::GLSurface::GetCurrent()),
      frame_available_event_(new FrameAvailableEvent()) {
  DCHECK(context_);
  DCHECK(surface_);
  surface_texture_->SetFrameAvailableCallbackOnAnyThread(base::BindRepeating(
      &FrameAvailableEvent::Signal, frame_available_event_));
}

SurfaceTextureGLOwner::~SurfaceTextureGLOwner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Make sure that the SurfaceTexture isn't using the GL objects.
  surface_texture_ = nullptr;

  ui::ScopedMakeCurrent scoped_make_current(context_.get(), surface_.get());
  if (scoped_make_current.Succeeded()) {
    glDeleteTextures(1, &texture_id_);
    DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }
}

GLuint SurfaceTextureGLOwner::GetTextureId() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return texture_id_;
}

gl::ScopedJavaSurface SurfaceTextureGLOwner::CreateJavaSurface() const {
  return gl::ScopedJavaSurface(surface_texture_.get());
}

void SurfaceTextureGLOwner::UpdateTexImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  surface_texture_->UpdateTexImage();
}

void SurfaceTextureGLOwner::GetTransformMatrix(float mtx[]) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  surface_texture_->GetTransformMatrix(mtx);
}

void SurfaceTextureGLOwner::ReleaseBackBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  surface_texture_->ReleaseBackBuffers();
}

gl::GLContext* SurfaceTextureGLOwner::GetContext() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return context_.get();
}

gl::GLSurface* SurfaceTextureGLOwner::GetSurface() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return surface_.get();
}

void SurfaceTextureGLOwner::SetReleaseTimeToNow() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  release_time_ = base::TimeTicks::Now();
}

void SurfaceTextureGLOwner::IgnorePendingRelease() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  release_time_ = base::TimeTicks();
}

bool SurfaceTextureGLOwner::IsExpectingFrameAvailable() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !release_time_.is_null();
}

void SurfaceTextureGLOwner::WaitForFrameAvailable() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!release_time_.is_null());

  // 5msec covers >99.9% of cases, so just wait for up to that much before
  // giving up. If an error occurs, we might not ever get a notification.
  const base::TimeDelta max_wait = base::TimeDelta::FromMilliseconds(5);
  const base::TimeTicks call_time = base::TimeTicks::Now();
  const base::TimeDelta elapsed = call_time - release_time_;
  const base::TimeDelta remaining = max_wait - elapsed;
  release_time_ = base::TimeTicks();

  if (remaining <= base::TimeDelta()) {
    if (!frame_available_event_->event.IsSignaled()) {
      DVLOG(1) << "Deferred WaitForFrameAvailable() timed out, elapsed: "
               << elapsed.InMillisecondsF() << "ms";
    }
    return;
  }

  DCHECK_LE(remaining, max_wait);
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AvdaCodecImage.WaitTimeForFrame");
  if (!frame_available_event_->event.TimedWait(remaining)) {
    DVLOG(1) << "WaitForFrameAvailable() timed out, elapsed: "
             << elapsed.InMillisecondsF()
             << "ms, additionally waited: " << remaining.InMillisecondsF()
             << "ms, total: " << (elapsed + remaining).InMillisecondsF()
             << "ms";
  }
}

}  // namespace media
