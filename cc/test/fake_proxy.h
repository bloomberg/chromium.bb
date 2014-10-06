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

  virtual void FinishAllRendering() override {}
  virtual bool IsStarted() const override;
  virtual void SetOutputSurface(scoped_ptr<OutputSurface>) override {}
  virtual void SetLayerTreeHostClientReady() override {}
  virtual void SetVisible(bool visible) override {}
  virtual const RendererCapabilities& GetRendererCapabilities() const override;
  virtual void SetNeedsAnimate() override {}
  virtual void SetNeedsUpdateLayers() override {}
  virtual void SetNeedsCommit() override {}
  virtual void SetNeedsRedraw(const gfx::Rect& damage_rect) override {}
  virtual void SetNextCommitWaitsForActivation() override {}
  virtual void NotifyInputThrottledUntilCommit() override {}
  virtual void SetDeferCommits(bool defer_commits) override {}
  virtual void MainThreadHasStoppedFlinging() override {}
  virtual bool BeginMainFrameRequested() const override;
  virtual bool CommitRequested() const override;
  virtual void Start() override {}
  virtual void Stop() override {}
  virtual void ForceSerializeOnSwapBuffers() override {}
  virtual size_t MaxPartialTextureUpdates() const override;
  virtual bool SupportsImplScrolling() const override;
  virtual void SetDebugState(const LayerTreeDebugState& debug_state) override {}
  virtual bool MainFrameWillHappenForTesting() override;
  virtual void AsValueInto(base::debug::TracedValue* state) const override;

  virtual RendererCapabilities& GetRendererCapabilities();
  void SetMaxPartialTextureUpdates(size_t max);

 private:
  RendererCapabilities capabilities_;
  size_t max_partial_texture_updates_;
  LayerTreeHost* layer_tree_host_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PROXY_H_
