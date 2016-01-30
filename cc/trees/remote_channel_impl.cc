// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/remote_channel_impl.h"

#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "cc/animation/animation_events.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/proto/compositor_message_to_impl.pb.h"
#include "cc/proto/compositor_message_to_main.pb.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

scoped_ptr<RemoteChannelImpl> RemoteChannelImpl::Create(
    LayerTreeHost* layer_tree_host,
    RemoteProtoChannel* remote_proto_channel,
    TaskRunnerProvider* task_runner_provider) {
  return make_scoped_ptr(new RemoteChannelImpl(
      layer_tree_host, remote_proto_channel, task_runner_provider));
}

RemoteChannelImpl::RemoteChannelImpl(LayerTreeHost* layer_tree_host,
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

scoped_ptr<ProxyImpl> RemoteChannelImpl::CreateProxyImpl(
    ChannelImpl* channel_impl,
    LayerTreeHost* layer_tree_host,
    TaskRunnerProvider* task_runner_provider,
    scoped_ptr<BeginFrameSource> external_begin_frame_source) {
  DCHECK(task_runner_provider_->IsImplThread());
  DCHECK(!external_begin_frame_source);
  return ProxyImpl::Create(channel_impl, layer_tree_host, task_runner_provider,
                           std::move(external_begin_frame_source));
}

void RemoteChannelImpl::OnProtoReceived(
    scoped_ptr<proto::CompositorMessage> proto) {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(main().started);
  DCHECK(proto->has_to_impl());

  HandleProto(proto->to_impl());
}

void RemoteChannelImpl::HandleProto(
    const proto::CompositorMessageToImpl& proto) {
  DCHECK(task_runner_provider_->IsMainThread());

  switch (proto.message_type()) {
    case proto::CompositorMessageToImpl::
        MAIN_THREAD_HAS_STOPPED_FLINGING_ON_IMPL:
      ImplThreadTaskRunner()->PostTask(
          FROM_HERE, base::Bind(&ProxyImpl::MainThreadHasStoppedFlingingOnImpl,
                                proxy_impl_weak_ptr_));
      break;
    default:
      // TODO(khushalsagar): Add more types here.
      NOTIMPLEMENTED();
  }
}

void RemoteChannelImpl::FinishAllRendering() {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

bool RemoteChannelImpl::IsStarted() const {
  DCHECK(task_runner_provider_->IsMainThread());
  return main().started;
}

bool RemoteChannelImpl::CommitToActiveTree() const {
  return false;
}

void RemoteChannelImpl::SetOutputSurface(OutputSurface* output_surface) {
  DCHECK(task_runner_provider_->IsMainThread());

  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::InitializeOutputSurfaceOnImpl,
                            proxy_impl_weak_ptr_, output_surface));
}

void RemoteChannelImpl::ReleaseOutputSurface() {
  DCHECK(task_runner_provider_->IsMainThread());
  CompletionEvent completion;
  DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::ReleaseOutputSurfaceOnImpl,
                            proxy_impl_weak_ptr_, &completion));
  completion.Wait();
}

void RemoteChannelImpl::SetVisible(bool visible) {
  DCHECK(task_runner_provider_->IsMainThread());

  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::SetVisibleOnImpl, proxy_impl_weak_ptr_, visible));
}

void RemoteChannelImpl::SetThrottleFrameProduction(bool throttle) {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

const RendererCapabilities& RemoteChannelImpl::GetRendererCapabilities() const {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
  return main().renderer_capabilities;
}

void RemoteChannelImpl::SetNeedsAnimate() {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

void RemoteChannelImpl::SetNeedsUpdateLayers() {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

void RemoteChannelImpl::SetNeedsCommit() {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
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

void RemoteChannelImpl::Start(
    scoped_ptr<BeginFrameSource> external_begin_frame_source) {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(!main().started);
  DCHECK(!external_begin_frame_source);

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

bool RemoteChannelImpl::SupportsImplScrolling() const {
  return true;
}

void RemoteChannelImpl::SetChildrenNeedBeginFrames(
    bool children_need_begin_frames) {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

void RemoteChannelImpl::SetAuthoritativeVSyncInterval(
    const base::TimeDelta& interval) {
  NOTREACHED() << "Should not be called on the remote client LayerTreeHost";
}

void RemoteChannelImpl::UpdateTopControlsState(TopControlsState constraints,
                                               TopControlsState current,
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

void RemoteChannelImpl::DidCompleteSwapBuffers() {}

void RemoteChannelImpl::SetRendererCapabilitiesMainCopy(
    const RendererCapabilities& capabilities) {}

void RemoteChannelImpl::BeginMainFrameNotExpectedSoon() {}

void RemoteChannelImpl::DidCommitAndDrawFrame() {}

void RemoteChannelImpl::SetAnimationEvents(scoped_ptr<AnimationEvents> queue) {}

void RemoteChannelImpl::DidLoseOutputSurface() {
  DCHECK(task_runner_provider_->IsImplThread());

  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&RemoteChannelImpl::DidLoseOutputSurfaceOnMain,
                            impl().remote_channel_weak_ptr));
}

void RemoteChannelImpl::RequestNewOutputSurface() {
  DCHECK(task_runner_provider_->IsImplThread());

  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&RemoteChannelImpl::RequestNewOutputSurfaceOnMain,
                            impl().remote_channel_weak_ptr));
}

void RemoteChannelImpl::DidInitializeOutputSurface(
    bool success,
    const RendererCapabilities& capabilities) {
  DCHECK(task_runner_provider_->IsImplThread());

  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RemoteChannelImpl::DidInitializeOutputSurfaceOnMain,
                 impl().remote_channel_weak_ptr, success, capabilities));
}

void RemoteChannelImpl::DidCompletePageScaleAnimation() {}

void RemoteChannelImpl::PostFrameTimingEventsOnMain(
    scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
    scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events) {}

void RemoteChannelImpl::BeginMainFrame(
    scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {}

void RemoteChannelImpl::DidLoseOutputSurfaceOnMain() {
  DCHECK(task_runner_provider_->IsMainThread());

  main().layer_tree_host->DidLoseOutputSurface();
}

void RemoteChannelImpl::RequestNewOutputSurfaceOnMain() {
  DCHECK(task_runner_provider_->IsMainThread());

  main().layer_tree_host->RequestNewOutputSurface();
}

void RemoteChannelImpl::DidInitializeOutputSurfaceOnMain(
    bool success,
    const RendererCapabilities& capabilities) {
  DCHECK(task_runner_provider_->IsMainThread());

  if (!success) {
    main().layer_tree_host->DidFailToInitializeOutputSurface();
    return;
  }

  main().renderer_capabilities = capabilities;
  main().layer_tree_host->DidInitializeOutputSurface();
}

void RemoteChannelImpl::InitializeImplOnImpl(CompletionEvent* completion,
                                             LayerTreeHost* layer_tree_host) {
  DCHECK(task_runner_provider_->IsMainThreadBlocked());
  DCHECK(task_runner_provider_->IsImplThread());

  impl().proxy_impl =
      CreateProxyImpl(this, layer_tree_host, task_runner_provider_, nullptr);
  impl().proxy_impl_weak_factory = make_scoped_ptr(
      new base::WeakPtrFactory<ProxyImpl>(impl().proxy_impl.get()));
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
    LayerTreeHost* layer_tree_host,
    RemoteProtoChannel* remote_proto_channel)
    : layer_tree_host(layer_tree_host),
      remote_proto_channel(remote_proto_channel),
      started(false),
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
