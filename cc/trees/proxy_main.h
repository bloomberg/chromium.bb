// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_MAIN_H_
#define CC_TREES_PROXY_MAIN_H_

#include "base/memory/weak_ptr.h"
#include "cc/animation/animation_events.h"
#include "cc/base/cc_export.h"
#include "cc/debug/frame_timing_tracker.h"
#include "cc/output/renderer_capabilities.h"
#include "cc/trees/proxy_common.h"

namespace cc {
class ThreadedChannel;

// TODO(khushalsagar): The main side of ThreadProxy. It is currently defined as
// an interface with the implementation provided by ThreadProxy and will be
// made an independent class.
// The methods added to this interface should only use the MainThreadOnly or
// BlockedMainThread variables from ThreadProxy.
// See crbug/527200.
class CC_EXPORT ProxyMain {
 public:
  // TODO(khushalsagar): Make this ChannelMain*. When ProxyMain and
  // ProxyImpl are split, ProxyImpl will be passed a reference to ChannelImpl
  // at creation. Right now we just set it directly from ThreadedChannel
  // when the impl side is initialized.
  virtual void SetChannel(scoped_ptr<ThreadedChannel> threaded_channel) = 0;

 protected:
  virtual ~ProxyMain() {}

 private:
  friend class ThreadedChannel;
  // Callback for main side commands received from the Channel.
  virtual void DidCompleteSwapBuffers() = 0;
  virtual void SetRendererCapabilitiesMainCopy(
      const RendererCapabilities& capabilities) = 0;
  virtual void BeginMainFrameNotExpectedSoon() = 0;
  virtual void DidCommitAndDrawFrame() = 0;
  virtual void SetAnimationEvents(scoped_ptr<AnimationEventsVector> queue) = 0;
  virtual void DidLoseOutputSurface() = 0;
  virtual void RequestNewOutputSurface() = 0;
  virtual void DidInitializeOutputSurface(
      bool success,
      const RendererCapabilities& capabilities) = 0;
  virtual void DidCompletePageScaleAnimation() = 0;
  virtual void PostFrameTimingEventsOnMain(
      scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events) = 0;
  virtual void BeginMainFrame(
      scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) = 0;

  // TODO(khushalsagar): Rename as GetWeakPtr() once ThreadProxy is split.
  virtual base::WeakPtr<ProxyMain> GetMainWeakPtr() = 0;
};

}  // namespace cc

#endif  // CC_TREES_PROXY_MAIN_H_
