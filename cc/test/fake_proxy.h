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
  FakeProxy() : layer_tree_host_(nullptr) {}

  void SetLayerTreeHost(LayerTreeHost* host);

  bool IsStarted() const override;
  bool CommitToActiveTree() const override;
  void SetCompositorFrameSink(
      CompositorFrameSink* compositor_frame_sink) override {}
  void ReleaseCompositorFrameSink() override;
  void SetVisible(bool visible) override {}
  void SetNeedsAnimate() override {}
  void SetNeedsUpdateLayers() override {}
  void SetNeedsCommit() override {}
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override {}
  void SetNextCommitWaitsForActivation() override {}
  void NotifyInputThrottledUntilCommit() override {}
  void SetDeferCommits(bool defer_commits) override {}
  void MainThreadHasStoppedFlinging() override {}
  bool CommitRequested() const override;
  void Start() override {}
  void Stop() override {}
  void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  bool SupportsImplScrolling() const override;
  bool MainFrameWillHappenForTesting() override;
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override {}

 private:
  LayerTreeHost* layer_tree_host_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PROXY_H_
