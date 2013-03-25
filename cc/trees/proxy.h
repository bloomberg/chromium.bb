// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_H_
#define CC_TREES_PROXY_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/values.h"
#include "cc/base/cc_export.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace gfx {
class Rect;
class Vector2d;
}

namespace cc {

class Thread;
struct RendererCapabilities;

// Abstract class responsible for proxying commands from the main-thread side of
// the compositor over to the compositor implementation.
class CC_EXPORT Proxy {
 public:
  Thread* MainThread() const;
  bool HasImplThread() const;
  Thread* ImplThread() const;

  // Returns 0 if the current thread is neither the main thread nor the impl
  // thread.
  Thread* CurrentThread() const;

  // Debug hooks.
  bool IsMainThread() const;
  bool IsImplThread() const;
  bool IsMainThreadBlocked() const;
#ifndef NDEBUG
  void SetMainThreadBlocked(bool is_main_thread_blocked);
  void SetCurrentThreadIsImplThread(bool is_impl_thread);
#endif

  virtual ~Proxy();

  virtual bool CompositeAndReadback(void* pixels, gfx::Rect rect) = 0;

  virtual void StartPageScaleAnimation(gfx::Vector2d target_offset,
                                       bool use_anchor,
                                       float scale,
                                       base::TimeDelta duration) = 0;

  virtual void FinishAllRendering() = 0;

  virtual bool IsStarted() const = 0;

  // Attempts to initialize a context to use for rendering. Returns false if
  // the context could not be created.  The context will not be used and no
  // frames may be produced until InitializeRenderer() is called.
  virtual bool InitializeOutputSurface() = 0;

  // Indicates that the compositing surface associated with our context is
  // ready to use.
  virtual void SetSurfaceReady() = 0;

  virtual void SetVisible(bool visible) = 0;

  // Attempts to initialize the layer renderer. Returns false if the context
  // isn't usable for compositing.
  virtual bool InitializeRenderer() = 0;

  // Attempts to recreate the context and layer renderer after a context lost.
  // Returns false if the renderer couldn't be reinitialized.
  virtual bool RecreateOutputSurface() = 0;

  virtual const RendererCapabilities& GetRendererCapabilities() const = 0;

  virtual void SetNeedsAnimate() = 0;
  virtual void SetNeedsCommit() = 0;
  virtual void SetNeedsRedraw() = 0;

  // Defers commits until it is reset. It is only supported when in threaded
  // mode. It's an error to make a sync call like CompositeAndReadback while
  // commits are deferred.
  virtual void SetDeferCommits(bool defer_commits) = 0;

  virtual void MainThreadHasStoppedFlinging() = 0;

  virtual bool CommitRequested() const = 0;

  virtual void Start() = 0;  // Must be called before using the proxy.
  virtual void Stop() = 0;   // Must be called before deleting the proxy.

  // Forces 3D commands on all contexts to wait for all previous SwapBuffers
  // to finish before executing in the GPU process.
  virtual void ForceSerializeOnSwapBuffers() = 0;

  // Maximum number of sub-region texture updates supported for each commit.
  virtual size_t MaxPartialTextureUpdates() const = 0;

  virtual void AcquireLayerTextures() = 0;

  virtual skia::RefPtr<SkPicture> CapturePicture() = 0;
  virtual scoped_ptr<base::Value> AsValue() const = 0;

  // Testing hooks
  virtual bool CommitPendingForTesting() = 0;

 protected:
  explicit Proxy(scoped_ptr<Thread> impl_thread);
  friend class DebugScopedSetImplThread;
  friend class DebugScopedSetMainThread;
  friend class DebugScopedSetMainThreadBlocked;

 private:
  DISALLOW_COPY_AND_ASSIGN(Proxy);

  scoped_ptr<Thread> main_thread_;
  scoped_ptr<Thread> impl_thread_;
#ifndef NDEBUG
  bool impl_thread_is_overridden_;
  bool is_main_thread_blocked_;
#endif
};

#ifndef NDEBUG
class DebugScopedSetMainThreadBlocked {
 public:
  explicit DebugScopedSetMainThreadBlocked(Proxy* proxy) : proxy_(proxy) {
    DCHECK(!proxy_->IsMainThreadBlocked());
    proxy_->SetMainThreadBlocked(true);
  }
  ~DebugScopedSetMainThreadBlocked() {
    DCHECK(proxy_->IsMainThreadBlocked());
    proxy_->SetMainThreadBlocked(false);
  }
 private:
  Proxy* proxy_;
};
#else
class DebugScopedSetMainThreadBlocked {
 public:
  explicit DebugScopedSetMainThreadBlocked(Proxy* proxy) {}
  ~DebugScopedSetMainThreadBlocked() {}
};
#endif

}

#endif  // CC_TREES_PROXY_H_
