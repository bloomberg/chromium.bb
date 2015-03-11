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

  void FinishAllRendering() override {}
  bool IsStarted() const override;
  bool CommitToActiveTree() const override;
  void SetOutputSurface(scoped_ptr<OutputSurface>) override {}
  void SetLayerTreeHostClientReady() override {}
  void SetVisible(bool visible) override {}
  void SetThrottleFrameProduction(bool throttle) override {}
  const RendererCapabilities& GetRendererCapabilities() const override;
  void SetNeedsAnimate() override {}
  void SetNeedsUpdateLayers() override {}
  void SetNeedsCommit() override {}
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override {}
  void SetNextCommitWaitsForActivation() override {}
  void NotifyInputThrottledUntilCommit() override {}
  void SetDeferCommits(bool defer_commits) override {}
  void MainThreadHasStoppedFlinging() override {}
  bool BeginMainFrameRequested() const override;
  bool CommitRequested() const override;
  void Start() override {}
  void Stop() override {}
  void ForceSerializeOnSwapBuffers() override {}
  size_t MaxPartialTextureUpdates() const override;
  bool SupportsImplScrolling() const override;
  void SetDebugState(const LayerTreeDebugState& debug_state) override {}
  bool MainFrameWillHappenForTesting() override;
  void SetChildrenNeedBeginFrames(bool children_need_begin_frames) override {}

  virtual RendererCapabilities& GetRendererCapabilities();
  void SetMaxPartialTextureUpdates(size_t max);

 private:
  RendererCapabilities capabilities_;
  size_t max_partial_texture_updates_;
  LayerTreeHost* layer_tree_host_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PROXY_H_
