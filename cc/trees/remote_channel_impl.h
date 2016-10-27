// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_REMOTE_CHANNEL_IMPL_H_
#define CC_TREES_REMOTE_CHANNEL_IMPL_H_

#include <queue>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/completion_event.h"
#include "cc/proto/compositor_message_to_impl.pb.h"
#include "cc/trees/channel_impl.h"
#include "cc/trees/proxy.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/remote_proto_channel.h"

namespace cc {
class LayerTreeHostInProcess;

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
//                                                   CompositorFrameSinkCreation
//                                                          |
//                                    ChannelImpl::RequestNewCompositorFrameSink
//                                                          |
// RemoteChannelImpl::                                      |
//      RequestNewCompositorFrameSink()<----PostTask--------------
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
// the impl thread which may be directed to the LayerTreeHostInProcesson the
// client (for instance output surface requests) or sent to the
// LayerTreeHostInProcess on the server. The messages to the server are created
// on the impl thread and sent using the RemoteProtoChannel on the main thread.
class CC_EXPORT RemoteChannelImpl : public ChannelImpl,
                                    public RemoteProtoChannel::ProtoReceiver,
                                    public Proxy {
 public:
  RemoteChannelImpl(LayerTreeHostInProcess* layer_tree_host,
                    RemoteProtoChannel* remote_proto_channel,
                    TaskRunnerProvider* task_runner_provider);
  ~RemoteChannelImpl() override;

  // virtual for testing.
  virtual std::unique_ptr<ProxyImpl> CreateProxyImpl(
      ChannelImpl* channel_impl,
      LayerTreeHostInProcess* layer_tree_host,
      TaskRunnerProvider* task_runner_provider);

 private:
  struct MainThreadOnly {
    LayerTreeHostInProcess* layer_tree_host;
    RemoteProtoChannel* remote_proto_channel;

    bool started;

    // This is set to true if we lost the output surface and can not push any
    // commits to the impl thread.
    bool waiting_for_compositor_frame_sink_initialization;

    // The queue of messages received from the server. The messages are added to
    // this queue if we are waiting for a new output surface to be initialized.
    std::queue<proto::CompositorMessageToImpl> pending_messages;

    base::WeakPtrFactory<RemoteChannelImpl> remote_channel_weak_factory;

    MainThreadOnly(RemoteChannelImpl*,
                   LayerTreeHostInProcess* layer_tree_host,
                   RemoteProtoChannel* remote_proto_channel);
    ~MainThreadOnly();
  };

  struct CompositorThreadOnly {
    std::unique_ptr<ProxyImpl> proxy_impl;
    std::unique_ptr<base::WeakPtrFactory<ProxyImpl>> proxy_impl_weak_factory;
    base::WeakPtr<RemoteChannelImpl> remote_channel_weak_ptr;

    CompositorThreadOnly(
        base::WeakPtr<RemoteChannelImpl> remote_channel_weak_ptr);
    ~CompositorThreadOnly();
  };

  // called on main thread.
  // RemoteProtoChannel::ProtoReceiver implementation.
  void OnProtoReceived(
      std::unique_ptr<proto::CompositorMessage> proto) override;

  // Proxy implementation
  bool IsStarted() const override;
  bool CommitToActiveTree() const override;
  void SetCompositorFrameSink(
      CompositorFrameSink* compositor_frame_sink) override;
  void ReleaseCompositorFrameSink() override;
  void SetVisible(bool visible) override;
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
  void Start() override;
  void Stop() override;
  void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  bool SupportsImplScrolling() const override;
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override;
  bool MainFrameWillHappenForTesting() override;

  // Called on impl thread.
  // ChannelImpl implementation
  void DidReceiveCompositorFrameAck() override;
  void BeginMainFrameNotExpectedSoon() override;
  void DidCommitAndDrawFrame() override;
  void SetAnimationEvents(std::unique_ptr<AnimationEvents> queue) override;
  void DidLoseCompositorFrameSink() override;
  void RequestNewCompositorFrameSink() override;
  void DidInitializeCompositorFrameSink(bool success) override;
  void DidCompletePageScaleAnimation() override;
  void BeginMainFrame(std::unique_ptr<BeginMainFrameAndCommitState>
                          begin_main_frame_state) override;

  void SendMessageProto(std::unique_ptr<proto::CompositorMessage> proto);

  // called on main thread.
  void HandleProto(const proto::CompositorMessageToImpl& proto);
  void DidReceiveCompositorFrameAckOnMain();
  void DidCommitAndDrawFrameOnMain();
  void DidLoseCompositorFrameSinkOnMain();
  void RequestNewCompositorFrameSinkOnMain();
  void DidInitializeCompositorFrameSinkOnMain(bool success);
  void SendMessageProtoOnMain(std::unique_ptr<proto::CompositorMessage> proto);
  void PostSetNeedsRedrawToImpl(const gfx::Rect& damaged_rect);

  void InitializeImplOnImpl(CompletionEvent* completion,
                            LayerTreeHostInProcess* layer_tree_host);
  void ShutdownImplOnImpl(CompletionEvent* completion);

  MainThreadOnly& main();
  const MainThreadOnly& main() const;
  CompositorThreadOnly& impl();
  const CompositorThreadOnly& impl() const;

  base::SingleThreadTaskRunner* MainThreadTaskRunner() const;
  base::SingleThreadTaskRunner* ImplThreadTaskRunner() const;

  TaskRunnerProvider* task_runner_provider_;

  // use accessors instead of these variables directly
  MainThreadOnly main_thread_vars_unsafe_;
  CompositorThreadOnly compositor_thread_vars_unsafe_;

  base::WeakPtr<ProxyImpl> proxy_impl_weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(RemoteChannelImpl);
};

}  // namespace cc

#endif  // CC_TREES_REMOTE_CHANNEL_IMPL_H_
