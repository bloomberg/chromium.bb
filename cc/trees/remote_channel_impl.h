// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_REMOTE_CHANNEL_IMPL_H_
#define CC_TREES_REMOTE_CHANNEL_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/completion_event.h"
#include "cc/trees/channel_impl.h"
#include "cc/trees/proxy.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/remote_proto_channel.h"

namespace cc {
class LayerTreeHost;

namespace proto {
class CompositorMessage;
class CompositorMessageToImpl;
class CompositorMessageToMain;
}

// The Proxy and ChannelImpl implementation for the remote compositor.
// The object life cycle and communication across threads is as follows:
//
// The RemoteChannelImpl is created by the remote client LayerTreeHost, which
// initializes the impl side of the compositor.
//
//           Main Thread              |               Impl Thread
//                                    |
//         Proxy::Start               |
//               |                    |
//               |              RemoteChannelImpl
// ---------------------------------------------------------------------------
// RemoteChannelImpl::Start()---PostTask---> RemoteChannelImpl::
//                                                    InitializeImplOnImpl
//                                                          |
//                                                   ProxyImpl::Create
//                                                          |
//                                                          .
//                                                          .
//                                          ProxyImpl::ScheduledActionBegin
//                                                     OutputSurfaceCreation
//                                                          |
//                                         ChannelImpl::RequestNewOutputSurface
//                                                          |
// RemoteChannelImpl::                                      |
//      RequestNewOutputSurface()<----PostTask--------------
//              .
//              .
//              |
// RemoteChannelImpl::
//          ~RemoteChannelImpl() ---PostTask---> RemoteChannelImpl::
//                                                      ShutdownImplOnImpl
// ----------------------------------------------------------------------------
//
// The class is created and destroyed on the main thread but can be safely
// called from the main or impl thread. It is safe to call RemoteChannelImpl on
// the impl thread since:
// 1) The only impl threaded callers of RemoteChannelImpl is the
// RemoteChannelImpl itself and ProxyImpl which is created and owned by the
// RemoteChannelImpl.
// 2) The RemoteChannelImpl blocks the main thread in its dtor to wait for the
// impl-thread teardown to complete, which ensures that any tasks queued to call
// it on the impl thread are run before it is destroyed on the main thread.
//
// The RemoteChannelImpl receives and processes messages from the remote server
// compositor on the main thread. The requests from ProxyImpl are received on
// the impl thread which may be directed to the LayerTreeHost on the client
// (for instance output surface requests) or sent to the LayerTreeHost on the
// server. The messages to the server are created on the impl thread and sent
// using the RemoteProtoChannel on the main thread.
class CC_EXPORT RemoteChannelImpl : public ChannelImpl,
                                    public RemoteProtoChannel::ProtoReceiver,
                                    public Proxy {
 public:
  static scoped_ptr<RemoteChannelImpl> Create(
      LayerTreeHost* layer_tree_host,
      RemoteProtoChannel* remote_proto_channel,
      TaskRunnerProvider* task_runner_provider);

  ~RemoteChannelImpl() override;

 protected:
  RemoteChannelImpl(LayerTreeHost* layer_tree_host,
                    RemoteProtoChannel* remote_proto_channel,
                    TaskRunnerProvider* task_runner_provider);

  // virtual for testing.
  virtual scoped_ptr<ProxyImpl> CreateProxyImpl(
      ChannelImpl* channel_impl,
      LayerTreeHost* layer_tree_host,
      TaskRunnerProvider* task_runner_provider,
      scoped_ptr<BeginFrameSource> external_begin_frame_source);

 private:
  struct MainThreadOnly {
    LayerTreeHost* layer_tree_host;
    RemoteProtoChannel* remote_proto_channel;

    bool started;

    RendererCapabilities renderer_capabilities;

    MainThreadOnly(LayerTreeHost* layer_tree_host,
                   RemoteProtoChannel* remote_proto_channel);
    ~MainThreadOnly();
  };

  struct CompositorThreadOnly {
    scoped_ptr<ProxyImpl> proxy_impl;
    scoped_ptr<base::WeakPtrFactory<ProxyImpl>> proxy_impl_weak_factory;

    CompositorThreadOnly();
    ~CompositorThreadOnly();
  };

  // called on main thread.
  // RemoteProtoChannel::ProtoReceiver implementation.
  void OnProtoReceived(scoped_ptr<proto::CompositorMessage> proto) override;

  // Proxy implementation
  void FinishAllRendering() override;
  bool IsStarted() const override;
  bool CommitToActiveTree() const override;
  void SetOutputSurface(OutputSurface* output_surface) override;
  void ReleaseOutputSurface() override;
  void SetVisible(bool visible) override;
  void SetThrottleFrameProduction(bool throttle) override;
  const RendererCapabilities& GetRendererCapabilities() const override;
  void SetNeedsAnimate() override;
  void SetNeedsUpdateLayers() override;
  void SetNeedsCommit() override;
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override;
  void SetNextCommitWaitsForActivation() override;
  void NotifyInputThrottledUntilCommit() override;
  void SetDeferCommits(bool defer_commits) override;
  void MainThreadHasStoppedFlinging() override;
  bool CommitRequested() const override;
  bool BeginMainFrameRequested() const override;
  void Start(scoped_ptr<BeginFrameSource> external_begin_frame_source) override;
  void Stop() override;
  bool SupportsImplScrolling() const override;
  void SetChildrenNeedBeginFrames(bool children_need_begin_frames) override;
  void SetAuthoritativeVSyncInterval(const base::TimeDelta& interval) override;
  void UpdateTopControlsState(TopControlsState constraints,
                              TopControlsState current,
                              bool animate) override;
  bool MainFrameWillHappenForTesting() override;

  // Called on impl thread.
  // ChannelImpl implementation
  void DidCompleteSwapBuffers() override;
  void SetRendererCapabilitiesMainCopy(
      const RendererCapabilities& capabilities) override;
  void BeginMainFrameNotExpectedSoon() override;
  void DidCommitAndDrawFrame() override;
  void SetAnimationEvents(scoped_ptr<AnimationEvents> queue) override;
  void DidLoseOutputSurface() override;
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface(
      bool success,
      const RendererCapabilities& capabilities) override;
  void DidCompletePageScaleAnimation() override;
  void PostFrameTimingEventsOnMain(
      scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events)
      override;
  void BeginMainFrame(
      scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) override;

  // called on main thread.
  void HandleProto(const proto::CompositorMessageToImpl& proto);

  void InitializeImplOnImpl(CompletionEvent* completion,
                            LayerTreeHost* layer_tree_host);
  void ShutdownImplOnImpl(CompletionEvent* completion);

  MainThreadOnly& main();
  const MainThreadOnly& main() const;
  CompositorThreadOnly& impl();
  const CompositorThreadOnly& impl() const;

  base::SingleThreadTaskRunner* MainThreadTaskRunner() const;
  base::SingleThreadTaskRunner* ImplThreadTaskRunner() const;

  // use accessors instead of these variables directly
  MainThreadOnly main_thread_vars_unsafe_;
  CompositorThreadOnly compositor_thread_vars_unsafe_;

  TaskRunnerProvider* task_runner_provider_;

  base::WeakPtr<ProxyImpl> proxy_impl_weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(RemoteChannelImpl);
};

}  // namespace cc

#endif  // CC_TREES_REMOTE_CHANNEL_IMPL_H_
