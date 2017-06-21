// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_SURFACE_TEXTURE_GL_OWNER_H_
#define MEDIA_GPU_SURFACE_TEXTURE_GL_OWNER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace media {

struct FrameAvailableEvent;

// A SurfaceTexture wrapper that creates and maintains ownership of the
// attached GL texture. The texture is destroyed with the object but it's
// possible to call ReleaseSurfaceTexture() without destroying the GL texture.
// It should only be accessed on the thread it was created on, with the
// exception of CreateJavaSurface(), which can be called on any thread.
// It's safe to keep and drop refptrs to it on any thread; it will be
// automatically destructed on the thread it was constructed on.
// Virtual for testing; see SurfaceTextureGLOwnerImpl.
class MEDIA_GPU_EXPORT SurfaceTextureGLOwner
    : public base::RefCountedDeleteOnSequence<SurfaceTextureGLOwner> {
 public:
  SurfaceTextureGLOwner();

  // Returns the GL texture id that the SurfaceTexture is attached to.
  virtual GLuint GetTextureId() const = 0;
  virtual gl::GLContext* GetContext() const = 0;
  virtual gl::GLSurface* GetSurface() const = 0;

  // Create a java surface for the SurfaceTexture.
  virtual gl::ScopedJavaSurface CreateJavaSurface() const = 0;

  // See gl::SurfaceTexture for the following.
  virtual void UpdateTexImage() = 0;
  virtual void GetTransformMatrix(float mtx[16]) = 0;
  virtual void ReleaseBackBuffers() = 0;

  // Sets the expectation of onFrameAVailable for a new frame because a buffer
  // was just released to this surface.
  virtual void SetReleaseTimeToNow() = 0;

  // Ignores a pending release that was previously indicated with
  // SetReleaseTimeToNow().
  // TODO(watk): This doesn't seem necessary. It actually may be detrimental
  // because the next time we release a buffer we may confuse its
  // onFrameAvailable with the one we're ignoring.
  virtual void IgnorePendingRelease() = 0;

  // Whether we're expecting onFrameAvailable. True when SetReleaseTimeToNow()
  // was called but neither IgnorePendingRelease() nor WaitForFrameAvailable()
  // have been called since.
  virtual bool IsExpectingFrameAvailable() = 0;

  // Waits for onFrameAvailable until it's been 5ms since the buffer was
  // released. This must only be called if IsExpectingFrameAvailable().
  virtual void WaitForFrameAvailable() = 0;

 protected:
  friend class base::RefCountedDeleteOnSequence<SurfaceTextureGLOwner>;
  friend class base::DeleteHelper<SurfaceTextureGLOwner>;
  virtual ~SurfaceTextureGLOwner() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceTextureGLOwner);
};

class MEDIA_GPU_EXPORT SurfaceTextureGLOwnerImpl
    : public SurfaceTextureGLOwner {
 public:
  // Creates a GL texture using the current platform GL context and returns a
  // new SurfaceTextureGLOwnerImpl attached to it. Returns null on failure.
  static scoped_refptr<SurfaceTextureGLOwner> Create();

  GLuint GetTextureId() const override;
  gl::GLContext* GetContext() const override;
  gl::GLSurface* GetSurface() const override;
  gl::ScopedJavaSurface CreateJavaSurface() const override;
  void UpdateTexImage() override;
  void GetTransformMatrix(float mtx[16]) override;
  void ReleaseBackBuffers() override;
  void SetReleaseTimeToNow() override;
  void IgnorePendingRelease() override;
  bool IsExpectingFrameAvailable() override;
  void WaitForFrameAvailable() override;

 private:
  SurfaceTextureGLOwnerImpl(GLuint texture_id);
  ~SurfaceTextureGLOwnerImpl() override;

  scoped_refptr<gl::SurfaceTexture> surface_texture_;
  GLuint texture_id_;

  // The context and surface that were used to create |texture_id_|.
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;

  // When SetReleaseTimeToNow() was last called. i.e., when the last
  // codec buffer was released to this surface. Or null if
  // IgnorePendingRelease() or WaitForFrameAvailable() have been called since.
  base::TimeTicks release_time_;
  scoped_refptr<FrameAvailableEvent> frame_available_event_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SurfaceTextureGLOwnerImpl);
};

}  // namespace media

#endif  // MEDIA_GPU_SURFACE_TEXTURE_GL_OWNER_H_
