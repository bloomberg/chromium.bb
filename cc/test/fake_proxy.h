// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PROXY_H_
#define CC_TEST_FAKE_PROXY_H_

#include "base/single_thread_task_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/proxy.h"

namespace cc {

class FakeProxy : public Proxy {
 public:
  FakeProxy() : Proxy(NULL, NULL), layer_tree_host_(NULL) {}
  explicit FakeProxy(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner)
      : Proxy(main_task_runner, impl_task_runner), layer_tree_host_(NULL) {}

  void SetLayerTreeHost(LayerTreeHost* host);

  virtual void FinishAllRendering() OVERRIDE {}
  virtual bool IsStarted() const OVERRIDE;
  virtual void SetLayerTreeHostClientReady() OVERRIDE {}
  virtual void SetVisible(bool visible) OVERRIDE {}
  virtual const RendererCapabilities& GetRendererCapabilities() const OVERRIDE;
  virtual void SetNeedsAnimate() OVERRIDE {}
  virtual void SetNeedsUpdateLayers() OVERRIDE {}
  virtual void SetNeedsCommit() OVERRIDE {}
  virtual void SetNeedsRedraw(const gfx::Rect& damage_rect) OVERRIDE {}
  virtual void SetNextCommitWaitsForActivation() OVERRIDE {}
  virtual void NotifyInputThrottledUntilCommit() OVERRIDE {}
  virtual void SetDeferCommits(bool defer_commits) OVERRIDE {}
  virtual void MainThreadHasStoppedFlinging() OVERRIDE {}
  virtual bool BeginMainFrameRequested() const OVERRIDE;
  virtual bool CommitRequested() const OVERRIDE;
  virtual void Start() OVERRIDE {}
  virtual void Stop() OVERRIDE {}
  virtual void ForceSerializeOnSwapBuffers() OVERRIDE {}
  virtual size_t MaxPartialTextureUpdates() const OVERRIDE;
  virtual bool SupportsImplScrolling() const OVERRIDE;
  virtual void SetDebugState(const LayerTreeDebugState& debug_state) OVERRIDE {}
  virtual bool CommitPendingForTesting() OVERRIDE;
  virtual scoped_ptr<base::Value> AsValue() const OVERRIDE;

  virtual RendererCapabilities& GetRendererCapabilities();
  void SetMaxPartialTextureUpdates(size_t max);

 private:
  RendererCapabilities capabilities_;
  size_t max_partial_texture_updates_;
  LayerTreeHost* layer_tree_host_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PROXY_H_
