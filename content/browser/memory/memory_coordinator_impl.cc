// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator_impl.h"

#include "base/memory/memory_coordinator_client_registry.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/memory/memory_monitor.h"
#include "content/browser/memory/memory_state_updater.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

namespace {

mojom::MemoryState ToMojomMemoryState(base::MemoryState state) {
  switch (state) {
    case base::MemoryState::UNKNOWN:
      return mojom::MemoryState::UNKNOWN;
    case base::MemoryState::NORMAL:
      return mojom::MemoryState::NORMAL;
    case base::MemoryState::THROTTLED:
      return mojom::MemoryState::THROTTLED;
    case base::MemoryState::SUSPENDED:
      return mojom::MemoryState::SUSPENDED;
    default:
      NOTREACHED();
      return mojom::MemoryState::UNKNOWN;
  }
}

}  // namespace

// The implementation of MemoryCoordinatorHandle. See memory_coordinator.mojom
// for the role of this class.
class MemoryCoordinatorHandleImpl : public mojom::MemoryCoordinatorHandle {
 public:
  MemoryCoordinatorHandleImpl(mojom::MemoryCoordinatorHandleRequest request,
                              MemoryCoordinatorImpl* coordinator,
                              int render_process_id);
  ~MemoryCoordinatorHandleImpl() override;

  // mojom::MemoryCoordinatorHandle:
  void AddChild(mojom::ChildMemoryCoordinatorPtr child) override;

  mojom::ChildMemoryCoordinatorPtr& child() { return child_; }
  mojo::Binding<mojom::MemoryCoordinatorHandle>& binding() { return binding_; }

 private:
  MemoryCoordinatorImpl* coordinator_;
  int render_process_id_;
  mojom::ChildMemoryCoordinatorPtr child_;
  mojo::Binding<mojom::MemoryCoordinatorHandle> binding_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorHandleImpl);
};

MemoryCoordinatorHandleImpl::MemoryCoordinatorHandleImpl(
    mojom::MemoryCoordinatorHandleRequest request,
    MemoryCoordinatorImpl* coordinator,
    int render_process_id)
    : coordinator_(coordinator),
      render_process_id_(render_process_id),
      binding_(this, std::move(request)) {
  DCHECK(coordinator_);
}

MemoryCoordinatorHandleImpl::~MemoryCoordinatorHandleImpl() {}

void MemoryCoordinatorHandleImpl::AddChild(
    mojom::ChildMemoryCoordinatorPtr child) {
  DCHECK(!child_.is_bound());
  child_ = std::move(child);
  coordinator_->OnChildAdded(render_process_id_);
}

// SingletonTraits for MemoryCoordinator. Returns MemoryCoordinatorImpl
// as an actual instance.
struct MemoryCoordinatorImplSingletonTraits
    : public base::LeakySingletonTraits<MemoryCoordinatorImpl> {
  static MemoryCoordinatorImpl* New() {
    return new MemoryCoordinatorImpl(base::ThreadTaskRunnerHandle::Get(),
                                     CreateMemoryMonitor());
  }
};

// static
MemoryCoordinator* MemoryCoordinator::GetInstance() {
  return MemoryCoordinatorImpl::GetInstance();
}

// static
MemoryCoordinatorImpl* MemoryCoordinatorImpl::GetInstance() {
  if (!base::FeatureList::IsEnabled(features::kMemoryCoordinator))
    return nullptr;
  return base::Singleton<MemoryCoordinatorImpl,
                         MemoryCoordinatorImplSingletonTraits>::get();
}

MemoryCoordinatorImpl::MemoryCoordinatorImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<MemoryMonitor> memory_monitor)
    : delegate_(GetContentClient()->browser()->GetMemoryCoordinatorDelegate()),
      memory_monitor_(std::move(memory_monitor)),
      state_updater_(base::MakeUnique<MemoryStateUpdater>(this, task_runner)) {
  DCHECK(memory_monitor_.get());
  base::MemoryCoordinatorProxy::SetMemoryCoordinator(this);
}

MemoryCoordinatorImpl::~MemoryCoordinatorImpl() {}

void MemoryCoordinatorImpl::Start() {
  DCHECK(CalledOnValidThread());
  DCHECK(last_state_change_.is_null());

  notification_registrar_.Add(
      this, NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      NotificationService::AllBrowserContextsAndSources());
  last_state_change_ = base::TimeTicks::Now();
  state_updater_->ScheduleUpdateState(base::TimeDelta());
}

void MemoryCoordinatorImpl::CreateHandle(
    int render_process_id,
    mojom::MemoryCoordinatorHandleRequest request) {
  std::unique_ptr<MemoryCoordinatorHandleImpl> handle(
      new MemoryCoordinatorHandleImpl(std::move(request), this,
                                      render_process_id));
  handle->binding().set_connection_error_handler(
      base::Bind(&MemoryCoordinatorImpl::OnConnectionError,
                 base::Unretained(this), render_process_id));
  CreateChildInfoMapEntry(render_process_id, std::move(handle));
}

bool MemoryCoordinatorImpl::SetChildMemoryState(int render_process_id,
                                                MemoryState memory_state) {
  // Can't set an invalid memory state.
  if (memory_state == MemoryState::UNKNOWN)
    return false;

  // Can't send a message to a child that doesn't exist.
  auto iter = children_.find(render_process_id);
  if (iter == children_.end())
    return false;

  // Can't send a message to a child that isn't bound.
  if (!iter->second.handle->child().is_bound())
    return false;

  memory_state = OverrideGlobalState(memory_state, iter->second);

  // A nop doesn't need to be sent, but is considered successful.
  if (iter->second.memory_state == memory_state)
    return true;

  // Can't suspend the given renderer.
  if (memory_state == MemoryState::SUSPENDED &&
      !CanSuspendRenderer(render_process_id))
    return false;

  // Update the internal state and send the message.
  iter->second.memory_state = memory_state;
  iter->second.handle->child()->OnStateChange(ToMojomMemoryState(memory_state));
  return true;
}

base::MemoryState MemoryCoordinatorImpl::GetChildMemoryState(
    int render_process_id) const {
  auto iter = children_.find(render_process_id);
  if (iter == children_.end())
    return base::MemoryState::UNKNOWN;
  return iter->second.memory_state;
}

void MemoryCoordinatorImpl::RecordMemoryPressure(
    base::MemoryPressureMonitor::MemoryPressureLevel level) {
  // TODO(bashi): Record memory pressure level.
}

base::MemoryState MemoryCoordinatorImpl::GetGlobalMemoryState() const {
  return current_state_;
}

base::MemoryState MemoryCoordinatorImpl::GetCurrentMemoryState() const {
  // SUSPENDED state may not make sense to the browser process. Use THROTTLED
  // instead when the global state is SUSPENDED.
  // TODO(bashi): Maybe worth considering another state for the browser.
  return current_state_ == MemoryState::SUSPENDED ? MemoryState::THROTTLED
                                                  : current_state_;
}

void MemoryCoordinatorImpl::SetCurrentMemoryStateForTesting(
    base::MemoryState memory_state) {
  // This changes the current state temporariy for testing. The state will be
  // updated 1 minute later.
  ForceSetGlobalState(memory_state, base::TimeDelta::FromMinutes(1));
}

void MemoryCoordinatorImpl::ForceSetGlobalState(base::MemoryState new_state,
                                                base::TimeDelta duration) {
  DCHECK(new_state != MemoryState::UNKNOWN);
  ChangeStateIfNeeded(current_state_, new_state);
  state_updater_->ScheduleUpdateState(duration);
}

void MemoryCoordinatorImpl::Observe(int type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  DCHECK(type == NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED);
  RenderWidgetHost* render_widget_host = Source<RenderWidgetHost>(source).ptr();
  RenderProcessHost* process = render_widget_host->GetProcess();
  if (!process)
    return;
  auto iter = children().find(process->GetID());
  if (iter == children().end())
    return;
  iter->second.is_visible = *Details<bool>(details).ptr();
  auto new_state = GetGlobalMemoryState();
  SetChildMemoryState(iter->first, new_state);
}

base::MemoryState MemoryCoordinatorImpl::GetStateForProcess(
    base::ProcessHandle handle) {
  DCHECK(CalledOnValidThread());
  if (handle == base::kNullProcessHandle)
    return MemoryState::UNKNOWN;
  if (handle == base::GetCurrentProcessHandle())
    return GetCurrentMemoryState();

  for (auto& iter : children()) {
    auto* render_process_host = GetRenderProcessHost(iter.first);
    if (render_process_host && render_process_host->GetHandle() == handle)
      return iter.second.memory_state;
  }
  return MemoryState::UNKNOWN;
}

bool MemoryCoordinatorImpl::ChangeStateIfNeeded(base::MemoryState prev_state,
                                                base::MemoryState next_state) {
  DCHECK(CalledOnValidThread());
  if (prev_state == next_state)
    return false;

  last_state_change_ = base::TimeTicks::Now();
  current_state_ = next_state;

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("memory-infra"),
               "MemoryCoordinatorImpl::ChangeStateIfNeeded", "prev",
               MemoryStateToString(prev_state), "next",
               MemoryStateToString(next_state));
  NotifyStateToClients();
  NotifyStateToChildren();
  return true;
}

void MemoryCoordinatorImpl::DiscardTab() {
  if (delegate_)
    delegate_->DiscardTab();
}

RenderProcessHost* MemoryCoordinatorImpl::GetRenderProcessHost(
    int render_process_id) {
  return RenderProcessHost::FromID(render_process_id);
}

void MemoryCoordinatorImpl::SetDelegateForTesting(
    std::unique_ptr<MemoryCoordinatorDelegate> delegate) {
  CHECK(!delegate_);
  delegate_ = std::move(delegate);
}

void MemoryCoordinatorImpl::AddChildForTesting(
    int dummy_render_process_id, mojom::ChildMemoryCoordinatorPtr child) {
  mojom::MemoryCoordinatorHandlePtr mch;
  auto request = mojo::MakeRequest(&mch);
  std::unique_ptr<MemoryCoordinatorHandleImpl> handle(
      new MemoryCoordinatorHandleImpl(std::move(request), this,
                                      dummy_render_process_id));
  handle->AddChild(std::move(child));
  CreateChildInfoMapEntry(dummy_render_process_id, std::move(handle));
}

void MemoryCoordinatorImpl::OnConnectionError(int render_process_id) {
  children_.erase(render_process_id);
}

bool MemoryCoordinatorImpl::CanSuspendRenderer(int render_process_id) {
  auto* render_process_host = GetRenderProcessHost(render_process_id);
  if (!render_process_host || !render_process_host->IsProcessBackgrounded())
    return false;
  if (render_process_host->GetWorkerRefCount() > 0)
    return false;
  // Assumes that we can't suspend renderers if there is no delegate.
  if (!delegate_)
    return false;
  return delegate_->CanSuspendBackgroundedRenderer(render_process_id);
}

void MemoryCoordinatorImpl::OnChildAdded(int render_process_id) {
  // Populate the global state as an initial state of a newly created process.
  auto new_state = GetGlobalMemoryState();
  SetChildMemoryState(render_process_id, new_state);
}

base::MemoryState MemoryCoordinatorImpl::OverrideGlobalState(
    MemoryState memory_state,
    const ChildInfo& child) {
  // We don't suspend foreground renderers. Throttle them instead.
  if (child.is_visible && memory_state == MemoryState::SUSPENDED)
    return MemoryState::THROTTLED;
#if defined(OS_ANDROID)
  // On Android, we throttle background renderers immediately.
  // TODO(bashi): Create a specialized class of MemoryCoordinator for Android
  // and move this ifdef to the class.
  if (!child.is_visible && memory_state == MemoryState::NORMAL)
    return MemoryState::THROTTLED;
  // TODO(bashi): Suspend background renderers after a certain period of time.
#endif  // defined(OS_ANDROID)
  return memory_state;
}

void MemoryCoordinatorImpl::CreateChildInfoMapEntry(
    int render_process_id,
    std::unique_ptr<MemoryCoordinatorHandleImpl> handle) {
  auto& child_info = children_[render_process_id];
  // Process always start with normal memory state.
  // We'll set renderer's memory state to the current global state when the
  // corresponding renderer process is ready to communicate. Renderer processes
  // call AddChild() when they are ready.
  child_info.memory_state = MemoryState::NORMAL;
  child_info.is_visible = true;
  child_info.handle = std::move(handle);
}

void MemoryCoordinatorImpl::NotifyStateToClients() {
  auto state = GetCurrentMemoryState();
  base::MemoryCoordinatorClientRegistry::GetInstance()->Notify(state);
}

void MemoryCoordinatorImpl::NotifyStateToChildren() {
  // It's OK to call SetChildMemoryState() unconditionally because it checks
  // whether this state transition is valid.
  for (auto& iter : children())
    SetChildMemoryState(iter.first, current_state_);
}

MemoryCoordinatorImpl::ChildInfo::ChildInfo() {}

MemoryCoordinatorImpl::ChildInfo::ChildInfo(const ChildInfo& rhs) {
  // This is a nop, but exists for compatibility with STL containers.
}

MemoryCoordinatorImpl::ChildInfo::~ChildInfo() {}

}  // namespace content
