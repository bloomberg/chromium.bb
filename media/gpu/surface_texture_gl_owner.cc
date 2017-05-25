// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/surface_texture_gl_owner.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

// FrameAvailableEvent is a RefCounted wrapper for a WaitableEvent
// because it's not possible to put one in RefCountedData.
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

scoped_refptr<SurfaceTextureGLOwner> SurfaceTextureGLOwner::Create() {
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  if (!texture_id)
    return nullptr;

  return new SurfaceTextureGLOwner(texture_id);
}

SurfaceTextureGLOwner::SurfaceTextureGLOwner(GLuint texture_id)
    : SurfaceTexture(CreateJavaSurfaceTexture(texture_id)),
      context_(gl::GLContext::GetCurrent()),
      surface_(gl::GLSurface::GetCurrent()),
      texture_id_(texture_id),
      frame_available_event_(new FrameAvailableEvent()) {
  DCHECK(context_);
  DCHECK(surface_);
  SetFrameAvailableCallbackOnAnyThread(
      base::Bind(&FrameAvailableEvent::Signal, frame_available_event_));
}

SurfaceTextureGLOwner::~SurfaceTextureGLOwner() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Make sure that the SurfaceTexture isn't using the GL objects.
  DestroyJavaObject();

  ui::ScopedMakeCurrent scoped_make_current(context_.get(), surface_.get());
  if (scoped_make_current.Succeeded()) {
    glDeleteTextures(1, &texture_id_);
    DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }
}

void SurfaceTextureGLOwner::SetReleaseTimeToNow() {
  release_time_ = base::TimeTicks::Now();
}

void SurfaceTextureGLOwner::IgnorePendingRelease() {
  release_time_ = base::TimeTicks();
}

bool SurfaceTextureGLOwner::IsExpectingFrameAvailable() {
  return !release_time_.is_null();
}

void SurfaceTextureGLOwner::WaitForFrameAvailable() {
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

void SurfaceTextureGLOwner::AttachToGLContext() {
  NOTIMPLEMENTED();
}

void SurfaceTextureGLOwner::DetachFromGLContext() {
  NOTIMPLEMENTED();
}

}  // namespace media
