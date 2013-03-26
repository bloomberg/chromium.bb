// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PROXY_H_
#define CC_TEST_FAKE_PROXY_H_

#include "cc/base/thread.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/proxy.h"

namespace cc {

class FakeProxy : public Proxy {
 public:
  explicit FakeProxy(scoped_ptr<Thread> impl_thread)
      : Proxy(impl_thread.Pass()) {}

  virtual bool CompositeAndReadback(void* pixels, gfx::Rect rect) OVERRIDE;
  virtual void FinishAllRendering() OVERRIDE {}
  virtual bool IsStarted() const OVERRIDE;
  virtual bool InitializeOutputSurface() OVERRIDE;
  virtual void SetSurfaceReady() OVERRIDE {}
  virtual void SetVisible(bool visible) OVERRIDE {}
  virtual bool InitializeRenderer() OVERRIDE;
  virtual bool RecreateOutputSurface() OVERRIDE;
  virtual const RendererCapabilities& GetRendererCapabilities() const OVERRIDE;
  virtual void SetNeedsAnimate() OVERRIDE {}
  virtual void SetNeedsCommit() OVERRIDE {}
  virtual void SetNeedsRedraw() OVERRIDE {}
  virtual void SetDeferCommits(bool defer_commits) OVERRIDE {}
  virtual void MainThreadHasStoppedFlinging() OVERRIDE {}
  virtual bool CommitRequested() const OVERRIDE;
  virtual void Start() OVERRIDE {}
  virtual void Stop() OVERRIDE {}
  virtual void ForceSerializeOnSwapBuffers() OVERRIDE {}
  virtual size_t MaxPartialTextureUpdates() const OVERRIDE;
  virtual void AcquireLayerTextures() OVERRIDE {}
  virtual bool CommitPendingForTesting() OVERRIDE;
  virtual skia::RefPtr<SkPicture> CapturePicture() OVERRIDE;
  virtual scoped_ptr<base::Value> AsValue() const OVERRIDE;

  virtual RendererCapabilities& GetRendererCapabilities();
  void SetMaxPartialTextureUpdates(size_t max);

 private:
  RendererCapabilities capabilities_;
  size_t max_partial_texture_updates_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PROXY_H_
