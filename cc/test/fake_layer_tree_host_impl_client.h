// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_

#include "cc/output/begin_frame_args.h"
#include "cc/trees/layer_tree_host_impl.h"

namespace cc {

class FakeLayerTreeHostImplClient : public LayerTreeHostImplClient {
 public:
  // LayerTreeHostImplClient implementation.
  void DidLoseLayerTreeFrameSinkOnImplThread() override {}
  void SetBeginFrameSource(BeginFrameSource* source) override {}
  void DidReceiveCompositorFrameAckOnImplThread() override {}
  void OnCanDrawStateChanged(bool can_draw) override {}
  void NotifyReadyToActivate() override {}
  void NotifyReadyToDraw() override {}
  void SetNeedsRedrawOnImplThread() override {}
  void SetNeedsOneBeginImplFrameOnImplThread() override {}
  void SetNeedsCommitOnImplThread() override {}
  void SetNeedsPrepareTilesOnImplThread() override {}
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override {}
  void PostAnimationEventsToMainThreadOnImplThread(
      std::unique_ptr<MutatorEvents> events) override;
  bool IsInsideDraw() override;
  void RenewTreePriority() override {}
  void PostDelayedAnimationTaskOnImplThread(const base::Closure& task,
                                            base::TimeDelta delay) override {}
  void DidActivateSyncTree() override {}
  void WillPrepareTiles() override {}
  void DidPrepareTiles() override {}
  void DidCompletePageScaleAnimationOnImplThread() override {}
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw) override {}
  void NeedsImplSideInvalidation() override;
  void RequestBeginMainFrameNotExpected(bool new_state) override {}
  void NotifyImageDecodeRequestFinished() override {}

  void reset_did_request_impl_side_invalidation() {
    did_request_impl_side_invalidation_ = false;
  }
  bool did_request_impl_side_invalidation() const {
    return did_request_impl_side_invalidation_;
  }

 private:
  bool did_request_impl_side_invalidation_ = false;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_
