// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/surface_texture_gl_owner.h"

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/abstract_texture.h"
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

SurfaceTextureGLOwner::SurfaceTextureGLOwner(
    std::unique_ptr<gpu::gles2::AbstractTexture> texture)
    : TextureOwner(true /*binds_texture_on_update */, std::move(texture)),
      surface_texture_(gl::SurfaceTexture::Create(GetTextureId())),
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

  // Clear the texture before we return, so that it can OnTextureDestroyed() if
  // it hasn't already.
  ClearAbstractTexture();
}

void SurfaceTextureGLOwner::OnTextureDestroyed(gpu::gles2::AbstractTexture*) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Make sure that the SurfaceTexture isn't using the GL objects.
  surface_texture_ = nullptr;
}

gl::ScopedJavaSurface SurfaceTextureGLOwner::CreateJavaSurface() const {
  // |surface_texture_| might be null, but that's okay.
  return gl::ScopedJavaSurface(surface_texture_.get());
}

void SurfaceTextureGLOwner::UpdateTexImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (surface_texture_)
    surface_texture_->UpdateTexImage();
}

void SurfaceTextureGLOwner::EnsureTexImageBound() {
  NOTREACHED();
}

void SurfaceTextureGLOwner::GetTransformMatrix(float mtx[]) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If we don't have a SurfaceTexture, then the matrix doesn't matter.  We
  // still initialize it for good measure.
  if (surface_texture_)
    surface_texture_->GetTransformMatrix(mtx);
  else
    memset(mtx, 0, sizeof(mtx[0]) * 16);
}

void SurfaceTextureGLOwner::ReleaseBackBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (surface_texture_)
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
  bool timed_out = false;

  if (remaining <= base::TimeDelta()) {
    if (!frame_available_event_->event.IsSignaled()) {
      DVLOG(1) << "Deferred WaitForFrameAvailable() timed out, elapsed: "
               << elapsed.InMillisecondsF() << "ms";
      timed_out = true;
    }
  } else {
    DCHECK_LE(remaining, max_wait);
    SCOPED_UMA_HISTOGRAM_TIMER(
        "Media.CodecImage.SurfaceTextureGLOwner.WaitTimeForFrame");
    if (!frame_available_event_->event.TimedWait(remaining)) {
      DVLOG(1) << "WaitForFrameAvailable() timed out, elapsed: "
               << elapsed.InMillisecondsF()
               << "ms, additionally waited: " << remaining.InMillisecondsF()
               << "ms, total: " << (elapsed + remaining).InMillisecondsF()
               << "ms";
      timed_out = true;
    }
  }
  UMA_HISTOGRAM_BOOLEAN("Media.CodecImage.SurfaceTextureGLOwner.FrameTimedOut",
                        timed_out);
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
SurfaceTextureGLOwner::GetAHardwareBuffer() {
  NOTREACHED() << "Don't use AHardwareBuffers with SurfaceTextureGLOwner";
  return nullptr;
}

}  // namespace media
