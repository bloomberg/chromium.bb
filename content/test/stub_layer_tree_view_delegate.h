// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_STUB_LAYER_TREE_VIEW_DELEGATE_H_
#define CONTENT_TEST_STUB_LAYER_TREE_VIEW_DELEGATE_H_

#include "cc/trees/element_id.h"
#include "content/renderer/compositor/layer_tree_view_delegate.h"

namespace cc {
struct ApplyViewportChangesArgs;
}

namespace content {

class StubLayerTreeViewDelegate : public LayerTreeViewDelegate {
 public:
  // LayerTreeViewDelegate implementation.
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs&) override {}
  void RecordWheelAndTouchScrollingCount(bool has_scrolled_by_wheel,
                                         bool has_scrolled_by_touch) override {}
  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) override {}
  void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) override {}
  void BeginMainFrame(base::TimeTicks frame_time) override {}
  void RecordEndOfFrameMetrics(base::TimeTicks) override {}
  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override;
  void DidCommitAndDrawCompositorFrame() override {}
  void DidCommitCompositorFrame() override {}
  void DidCompletePageScaleAnimation() override {}
  void UpdateVisualState(bool record_main_frame_metrics) override {}
  void WillBeginCompositorFrame() override {}
  std::unique_ptr<cc::SwapPromise> RequestCopyOfOutputForWebTest(
      std::unique_ptr<viz::CopyOutputRequest> request) override;
};

}  // namespace content

#endif  // CONTENT_TEST_STUB_LAYER_TREE_VIEW_DELEGATE_H_
