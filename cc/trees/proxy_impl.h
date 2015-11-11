// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_IMPL_H_
#define CC_TREES_PROXY_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/completion_event.h"
#include "cc/input/top_controls_state.h"
#include "cc/output/output_surface.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/trees/proxy_common.h"

namespace cc {
// TODO(khushalsagar): The impl side of ThreadProxy. It is currently defined as
// an interface with the implementation provided by ThreadProxy and will be
// made an independent class.
// The methods added to this interface should only use the CompositorThreadOnly
// variables from ThreadProxy.
// See crbug/527200
class CC_EXPORT ProxyImpl {
 private:
  friend class ThreadedChannel;

  // Callback for impl side commands received from the channel.
  virtual void SetThrottleFrameProductionOnImpl(bool throttle) = 0;
  virtual void UpdateTopControlsStateOnImpl(TopControlsState constraints,
                                            TopControlsState current,
                                            bool animate) = 0;
  virtual void InitializeOutputSurfaceOnImpl(OutputSurface* output_surface) = 0;
  virtual void MainThreadHasStoppedFlingingOnImpl() = 0;
  virtual void SetInputThrottledUntilCommitOnImpl(bool is_throttled) = 0;
  virtual void SetDeferCommitsOnImpl(bool defer_commits) const = 0;
  virtual void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) = 0;
  virtual void SetNeedsCommitOnImpl() = 0;
  virtual void FinishAllRenderingOnImpl(CompletionEvent* completion) = 0;
  virtual void SetVisibleOnImpl(bool visible) = 0;
  virtual void ReleaseOutputSurfaceOnImpl(CompletionEvent* completion) = 0;
  virtual void FinishGLOnImpl(CompletionEvent* completion) = 0;
  virtual void MainFrameWillHappenOnImplForTesting(
      CompletionEvent* completion,
      bool* main_frame_will_happen) = 0;
  virtual void BeginMainFrameAbortedOnImpl(
      CommitEarlyOutReason reason,
      base::TimeTicks main_thread_start_time) = 0;
  virtual void StartCommitOnImpl(CompletionEvent* completion,
                                 LayerTreeHost* layer_tree_host,
                                 base::TimeTicks main_thread_start_time,
                                 bool hold_commit_for_activation) = 0;
  virtual void InitializeImplOnImpl(CompletionEvent* completion,
                                    LayerTreeHost* layer_tree_host) = 0;
  virtual void LayerTreeHostClosedOnImpl(CompletionEvent* completion) = 0;

  // TODO(khushalsagar): Rename as GetWeakPtr() once ThreadProxy is split.
  virtual base::WeakPtr<ProxyImpl> GetImplWeakPtr() = 0;

 protected:
  virtual ~ProxyImpl() {}
};

}  // namespace cc

#endif  // CC_TREES_PROXY_IMPL_H_
