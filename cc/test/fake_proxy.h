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

  void FinishAllRendering() override {}
  bool IsStarted() const override;
  bool CommitToActiveTree() const override;
  void SetOutputSurface(OutputSurface* output_surface) override {}
  void ReleaseOutputSurface() override;
  void SetVisible(bool visible) override {}
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
  void Start(
      std::unique_ptr<BeginFrameSource> external_begin_frame_source) override {}
  void Stop() override {}
  bool SupportsImplScrolling() const override;
  bool MainFrameWillHappenForTesting() override;
  void SetAuthoritativeVSyncInterval(const base::TimeDelta& interval) override {
  }
  void UpdateTopControlsState(TopControlsState constraints,
                              TopControlsState current,
                              bool animate) override {}
  void SetOutputIsSecure(bool output_is_secure) override {}

  virtual RendererCapabilities& GetRendererCapabilities();

 private:
  RendererCapabilities capabilities_;
  LayerTreeHost* layer_tree_host_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PROXY_H_
