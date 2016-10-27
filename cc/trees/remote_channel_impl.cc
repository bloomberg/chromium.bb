// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/remote_channel_impl.h"

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "cc/animation/animation_events.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/proto/compositor_message_to_impl.pb.h"
#include "cc/proto/compositor_message_to_main.pb.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/trees/layer_tree_host_in_process.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

RemoteChannelImpl::RemoteChannelImpl(LayerTreeHostInProcess* layer_tree_host,
                                     RemoteProtoChannel* remote_proto_channel,
                                     TaskRunnerProvider* task_runner_provider)
    : task_runner_provider_(task_runner_provider),
      main_thread_vars_unsafe_(this, layer_tree_host, remote_proto_channel),
      compositor_thread_vars_unsafe_(
          main().remote_channel_weak_factory.GetWeakPtr()) {
  DCHECK(task_runner_provider_->IsMainThread());

  main().remote_proto_channel->SetProtoReceiver(this);
}

RemoteChannelImpl::~RemoteChannelImpl() {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(!main().started);

  main().remote_proto_channel->SetProtoReceiver(nullptr);
}

std::unique_ptr<ProxyImpl> RemoteChannelImpl::CreateProxyImpl(
    ChannelImpl* channel_impl,
    LayerTreeHostInProcess* layer_tree_host,
    TaskRunnerProvider* task_runner_provider) {
  DCHECK(task_runner_provider_->IsImplThread());
  return base::MakeUnique<ProxyImpl>(channel_impl, layer_tree_host,
                                     task_runner_provider);
}

void RemoteChannelImpl::OnProtoReceived(
    std::unique_ptr<proto::CompositorMessage> proto) {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(main().started);
  DCHECK(proto->has_to_impl());

  // If we don't have an CompositorFrameSink, queue the message and defer
  // processing it till we initialize a new CompositorFrameSink.
  if (main().waiting_for_compositor_frame_sink_initialization) {
    VLOG(1) << "Queueing message proto since CompositorFrameSink was released.";
    main().pending_messages.push(proto->to_impl());
  } else {
    HandleProto(proto->to_impl());
  }
}

void RemoteChannelImpl::HandleProto(
    const proto::CompositorMessageToImpl& proto) {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(proto.has_message_type());
  DCHECK(!main().waiting_for_compositor_frame_sink_initialization);

  switch (proto.message_type()) {
    case proto::CompositorMessageToImpl::UNKNOWN:
      NOTIMPLEMENTED() << "Ignoring message of UNKNOWN type";
      break;
    case proto::CompositorMessageToImpl::SET_NEEDS_COMMIT:
      VLOG(1) << "Received commit request from the engine.";
      ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&ProxyImpl::SetNeedsCommitOnImpl, proxy_impl_weak_ptr_));
      break;
    case proto::CompositorMessageToImpl::SET_DEFER_COMMITS: {
      const proto::SetDeferCommits& defer_commits_message =
          proto.defer_commits_message();
      bool defer_commits = defer_commits_message.defer_commits();
      VLOG(1) << "Received set defer commits to: " << defer_commits
              << " from the engine.";
      ImplThreadTaskRunner()->PostTask(
          FROM_HERE, base::Bind(&ProxyImpl::SetDeferCommitsOnImpl,
                                proxy_impl_weak_ptr_, defer_commits));
    } break;
    case proto::CompositorMessageToImpl::START_COMMIT: {
      VLOG(1) << "Received commit proto from the engine.";
      base::TimeTicks main_thread_start_time = base::TimeTicks::Now();
      const proto::StartCommit& start_commit_message =
          proto.start_commit_message();

      main().layer_tree_host->FromProtobufForCommit(
          start_commit_message.layer_tree_host());

      {
        DebugScopedSetMainThreadBlocked main_thread_blocked(
            task_runner_provider_);
        CompletionEvent completion;
        VLOG(1) << "Starting commit.";
        ImplThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&ProxyImpl::NotifyReadyToCommitOnImpl,
                       proxy_impl_weak_ptr_, &completion,
                       main().layer_tree_host, main_thread_start_time, false));
        completion.Wait();
      }
    } break;
    case proto::CompositorMessageToImpl::BEGIN_MAIN_FRAME_ABORTED: {
      base::TimeTicks main_thread_start_time = base::TimeTicks::Now();
      const proto::BeginMainFrameAborted& begin_main_frame_aborted_message =
          proto.begin_main_frame_aborted_message();
      CommitEarlyOutReason reason = CommitEarlyOutReasonFromProtobuf(
          begin_main_frame_aborted_message.reason());
      std::vector<std::unique_ptr<SwapPromise>> empty_swap_promises;
      VLOG(1) << "Received BeginMainFrameAborted from the engine with reason: "
              << CommitEarlyOutReasonToString(reason);
      ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&ProxyImpl::BeginMainFrameAbortedOnImpl,
                     proxy_impl_weak_ptr_, reason, main_thread_start_time,
                     base::Passed(&empty_swap_promises)));
    } break;
    case proto::CompositorMessageToImpl::SET_NEEDS_REDRAW: {
      VLOG(1) << "Received redraw request from the engine.";
      const proto::SetNeedsRedraw& set_needs_redraw_message =
          proto.set_needs_redraw_message();
      gfx::Rect damaged_rect =
          ProtoToRect(set_needs_redraw_message.damaged_rect());
      PostSetNeedsRedrawToImpl(damaged_rect);
    } break;
  }
}

bool RemoteChannelImpl::IsStarted() const {
  DCHECK(task_runner_provider_->IsMainThread());
  return main().started;
}

bool RemoteChannelImpl::CommitToActiveTree() const {
  return false;
}

void RemoteChannelImpl::SetCompositorFrameSink(
    CompositorFrameSink* compositor_frame_sink) {
  DCHECK(task_runner_provider_->IsMainThread());

  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::InitializeCompositorFrameSinkOnImpl,
                            proxy_impl_weak_ptr_, compositor_frame_sink));
}

void RemoteChannelImpl::ReleaseCompositorFrameSink() {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(!main().waiting_for_compositor_frame_sink_initialization);
  VLOG(1) << "Releasing CompositorFrameSink";

  {
    CompletionEvent completion;
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ProxyImpl::ReleaseCompositorFrameSinkOnImpl,
                              proxy_impl_weak_ptr_, &completion));
    completion.Wait();
  }

  main().waiting_for_compositor_frame_sink_initialization = true;
}

void RemoteChannelImpl::SetVisible(bool visible) {
  DCHECK(task_runner_provider_->IsMainThread());
  VLOG(1) << "Setting visibility to: " << visible;

  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::SetVisibleOnImpl, proxy_impl_weak_ptr_, visible));
}

void RemoteChannelImpl::SetNeedsAnimate() {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

void RemoteChannelImpl::SetNeedsUpdateLayers() {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

void RemoteChannelImpl::SetNeedsCommit() {
  // Ideally commits should be requested only on the server. But we have to
  // allow this call since the LayerTreeHostInProcesswill currently ask for a
  // commit in 2 cases:
  // 1) When it is being initialized from a protobuf for a commit.
  // 2) When it loses the CompositorFrameSink.
  NOTIMPLEMENTED() << "Commits should not be requested on the client";
}

void RemoteChannelImpl::SetNeedsRedraw(const gfx::Rect& damage_rect) {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

void RemoteChannelImpl::SetNextCommitWaitsForActivation() {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

void RemoteChannelImpl::NotifyInputThrottledUntilCommit() {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

void RemoteChannelImpl::SetDeferCommits(bool defer_commits) {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

void RemoteChannelImpl::MainThreadHasStoppedFlinging() {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

bool RemoteChannelImpl::CommitRequested() const {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
  return false;
}

bool RemoteChannelImpl::BeginMainFrameRequested() const {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
  return false;
}

void RemoteChannelImpl::Start() {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(!main().started);

  CompletionEvent completion;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&RemoteChannelImpl::InitializeImplOnImpl,
                              base::Unretained(this), &completion,
                              main().layer_tree_host));
    completion.Wait();
  }
  main().started = true;
}

void RemoteChannelImpl::Stop() {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(main().started);

  {
    CompletionEvent completion;
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ProxyImpl::FinishGLOnImpl, proxy_impl_weak_ptr_,
                              &completion));
    completion.Wait();
  }
  {
    CompletionEvent completion;
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&RemoteChannelImpl::ShutdownImplOnImpl,
                              base::Unretained(this), &completion));
    completion.Wait();
  }

  main().started = false;
  main().remote_channel_weak_factory.InvalidateWeakPtrs();
}

void RemoteChannelImpl::SetMutator(std::unique_ptr<LayerTreeMutator> mutator) {
  // TODO(vollick): add support for compositor worker.
}

bool RemoteChannelImpl::SupportsImplScrolling() const {
  return true;
}

void RemoteChannelImpl::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

bool RemoteChannelImpl::MainFrameWillHappenForTesting() {
  DCHECK(task_runner_provider_->IsMainThread());
  bool main_frame_will_happen;
  {
    CompletionEvent completion;
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ProxyImpl::MainFrameWillHappenOnImplForTesting,
                   proxy_impl_weak_ptr_, &completion, &main_frame_will_happen));
    completion.Wait();
  }
  return main_frame_will_happen;
}

void RemoteChannelImpl::DidReceiveCompositorFrameAck() {
  DCHECK(task_runner_provider_->IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RemoteChannelImpl::DidReceiveCompositorFrameAckOnMain,
                 impl().remote_channel_weak_ptr));
}

void RemoteChannelImpl::BeginMainFrameNotExpectedSoon() {}

void RemoteChannelImpl::DidCommitAndDrawFrame() {
  DCHECK(task_runner_provider_->IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&RemoteChannelImpl::DidCommitAndDrawFrameOnMain,
                            impl().remote_channel_weak_ptr));
}

void RemoteChannelImpl::SetAnimationEvents(
    std::unique_ptr<AnimationEvents> queue) {}

void RemoteChannelImpl::DidLoseCompositorFrameSink() {
  DCHECK(task_runner_provider_->IsImplThread());

  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RemoteChannelImpl::DidLoseCompositorFrameSinkOnMain,
                 impl().remote_channel_weak_ptr));
}

void RemoteChannelImpl::RequestNewCompositorFrameSink() {
  DCHECK(task_runner_provider_->IsImplThread());

  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RemoteChannelImpl::RequestNewCompositorFrameSinkOnMain,
                 impl().remote_channel_weak_ptr));
}

void RemoteChannelImpl::DidInitializeCompositorFrameSink(bool success) {
  DCHECK(task_runner_provider_->IsImplThread());

  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RemoteChannelImpl::DidInitializeCompositorFrameSinkOnMain,
                 impl().remote_channel_weak_ptr, success));
}

void RemoteChannelImpl::DidCompletePageScaleAnimation() {}

void RemoteChannelImpl::BeginMainFrame(
    std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {
  std::unique_ptr<proto::CompositorMessage> proto;
  proto.reset(new proto::CompositorMessage);
  proto::CompositorMessageToMain* to_main_proto = proto->mutable_to_main();

  to_main_proto->set_message_type(
      proto::CompositorMessageToMain::BEGIN_MAIN_FRAME);
  proto::BeginMainFrame* begin_main_frame_message =
      to_main_proto->mutable_begin_main_frame_message();
  begin_main_frame_state->ToProtobuf(
      begin_main_frame_message->mutable_begin_main_frame_state());

  SendMessageProto(std::move(proto));
}

void RemoteChannelImpl::SendMessageProto(
    std::unique_ptr<proto::CompositorMessage> proto) {
  DCHECK(task_runner_provider_->IsImplThread());

  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RemoteChannelImpl::SendMessageProtoOnMain,
                 impl().remote_channel_weak_ptr, base::Passed(&proto)));
}

void RemoteChannelImpl::DidReceiveCompositorFrameAckOnMain() {
  DCHECK(task_runner_provider_->IsMainThread());
  main().layer_tree_host->DidReceiveCompositorFrameAck();
}

void RemoteChannelImpl::DidCommitAndDrawFrameOnMain() {
  DCHECK(task_runner_provider_->IsMainThread());
  main().layer_tree_host->DidCommitAndDrawFrame();
}

void RemoteChannelImpl::DidLoseCompositorFrameSinkOnMain() {
  DCHECK(task_runner_provider_->IsMainThread());

  main().layer_tree_host->DidLoseCompositorFrameSink();
}

void RemoteChannelImpl::RequestNewCompositorFrameSinkOnMain() {
  DCHECK(task_runner_provider_->IsMainThread());

  main().layer_tree_host->RequestNewCompositorFrameSink();
}

void RemoteChannelImpl::DidInitializeCompositorFrameSinkOnMain(bool success) {
  DCHECK(task_runner_provider_->IsMainThread());

  if (!success) {
    main().layer_tree_host->DidFailToInitializeCompositorFrameSink();
    return;
  }

  VLOG(1) << "CompositorFrameSink initialized successfully";
  main().layer_tree_host->DidInitializeCompositorFrameSink();

  // If we were waiting for CompositorFrameSink initialization, we might have
  // queued some messages. Relay them now that a new CompositorFrameSink has
  // been initialized.
  main().waiting_for_compositor_frame_sink_initialization = false;
  while (!main().pending_messages.empty()) {
    VLOG(1) << "Handling queued message";
    HandleProto(main().pending_messages.front());
    main().pending_messages.pop();
  }

  // The commit after a new CompositorFrameSink can early out, in which case we
  // will never redraw. Schedule one just to be safe.
  PostSetNeedsRedrawToImpl(gfx::Rect(
      main().layer_tree_host->GetLayerTree()->device_viewport_size()));
}

void RemoteChannelImpl::SendMessageProtoOnMain(
    std::unique_ptr<proto::CompositorMessage> proto) {
  DCHECK(task_runner_provider_->IsMainThread());
  VLOG(1) << "Sending BeginMainFrame request to the engine.";

  main().remote_proto_channel->SendCompositorProto(*proto);
}

void RemoteChannelImpl::PostSetNeedsRedrawToImpl(
    const gfx::Rect& damaged_rect) {
  DCHECK(task_runner_provider_->IsMainThread());

  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::SetNeedsRedrawOnImpl,
                            proxy_impl_weak_ptr_, damaged_rect));
}

void RemoteChannelImpl::InitializeImplOnImpl(
    CompletionEvent* completion,
    LayerTreeHostInProcess* layer_tree_host) {
  DCHECK(task_runner_provider_->IsMainThreadBlocked());
  DCHECK(task_runner_provider_->IsImplThread());

  impl().proxy_impl =
      CreateProxyImpl(this, layer_tree_host, task_runner_provider_);
  impl().proxy_impl_weak_factory =
      base::MakeUnique<base::WeakPtrFactory<ProxyImpl>>(
          impl().proxy_impl.get());
  proxy_impl_weak_ptr_ = impl().proxy_impl_weak_factory->GetWeakPtr();
  completion->Signal();
}

void RemoteChannelImpl::ShutdownImplOnImpl(CompletionEvent* completion) {
  DCHECK(task_runner_provider_->IsMainThreadBlocked());
  DCHECK(task_runner_provider_->IsImplThread());

  // We must invalidate the proxy_impl weak ptrs and destroy the weak ptr
  // factory before destroying proxy_impl.
  impl().proxy_impl_weak_factory->InvalidateWeakPtrs();
  impl().proxy_impl_weak_factory.reset();

  impl().proxy_impl.reset();
  completion->Signal();
}

RemoteChannelImpl::MainThreadOnly& RemoteChannelImpl::main() {
  DCHECK(task_runner_provider_->IsMainThread());
  return main_thread_vars_unsafe_;
}

const RemoteChannelImpl::MainThreadOnly& RemoteChannelImpl::main() const {
  DCHECK(task_runner_provider_->IsMainThread());
  return main_thread_vars_unsafe_;
}

RemoteChannelImpl::CompositorThreadOnly& RemoteChannelImpl::impl() {
  DCHECK(task_runner_provider_->IsImplThread());
  return compositor_thread_vars_unsafe_;
}

const RemoteChannelImpl::CompositorThreadOnly& RemoteChannelImpl::impl() const {
  DCHECK(task_runner_provider_->IsImplThread());
  return compositor_thread_vars_unsafe_;
}

base::SingleThreadTaskRunner* RemoteChannelImpl::MainThreadTaskRunner() const {
  return task_runner_provider_->MainThreadTaskRunner();
}

base::SingleThreadTaskRunner* RemoteChannelImpl::ImplThreadTaskRunner() const {
  return task_runner_provider_->ImplThreadTaskRunner();
}

RemoteChannelImpl::MainThreadOnly::MainThreadOnly(
    RemoteChannelImpl* remote_channel_impl,
    LayerTreeHostInProcess* layer_tree_host,
    RemoteProtoChannel* remote_proto_channel)
    : layer_tree_host(layer_tree_host),
      remote_proto_channel(remote_proto_channel),
      started(false),
      waiting_for_compositor_frame_sink_initialization(false),
      remote_channel_weak_factory(remote_channel_impl) {
  DCHECK(layer_tree_host);
  DCHECK(remote_proto_channel);
}

RemoteChannelImpl::MainThreadOnly::~MainThreadOnly() {}

RemoteChannelImpl::CompositorThreadOnly::CompositorThreadOnly(
    base::WeakPtr<RemoteChannelImpl> remote_channel_weak_ptr)
    : proxy_impl(nullptr),
      proxy_impl_weak_factory(nullptr),
      remote_channel_weak_ptr(remote_channel_weak_ptr) {}

RemoteChannelImpl::CompositorThreadOnly::~CompositorThreadOnly() {}

}  // namespace cc
