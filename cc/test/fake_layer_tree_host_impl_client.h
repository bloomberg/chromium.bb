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
  virtual void UpdateRendererCapabilitiesOnImplThread() override {}
  virtual void DidLoseOutputSurfaceOnImplThread() override {}
  virtual void CommitVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) override {}
  virtual void SetEstimatedParentDrawTime(base::TimeDelta draw_time) override {}
  virtual void SetMaxSwapsPendingOnImplThread(int max) override {}
  virtual void DidSwapBuffersOnImplThread() override {}
  virtual void DidSwapBuffersCompleteOnImplThread() override {}
  virtual void OnCanDrawStateChanged(bool can_draw) override {}
  virtual void NotifyReadyToActivate() override {}
  virtual void SetNeedsRedrawOnImplThread() override {}
  virtual void SetNeedsRedrawRectOnImplThread(
    const gfx::Rect& damage_rect) override {}
  virtual void SetNeedsAnimateOnImplThread() override {}
  virtual void DidInitializeVisibleTileOnImplThread() override {}
  virtual void SetNeedsCommitOnImplThread() override {}
  virtual void SetNeedsManageTilesOnImplThread() override {}
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> events) override {}
  virtual bool ReduceContentsTextureMemoryOnImplThread(
      size_t limit_bytes,
      int priority_cutoff) override;
  virtual bool IsInsideDraw() override;
  virtual void RenewTreePriority() override {}
  virtual void PostDelayedScrollbarFadeOnImplThread(
      const base::Closure& start_fade,
      base::TimeDelta delay) override {}
  virtual void DidActivateSyncTree() override {}
  virtual void DidManageTiles() override {}
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_
