// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator_impl.h"

#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/memory/memory_monitor.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_features.h"

namespace content {

namespace {

// A expected renderer size. These values come from the median of appropriate
// UMA stats.
#if defined(OS_ANDROID) || defined(OS_IOS)
const int kDefaultExpectedRendererSizeMB = 40;
#elif defined(OS_WIN)
const int kDefaultExpectedRendererSizeMB = 70;
#else // Mac, Linux, and ChromeOS
const int kDefaultExpectedRendererSizeMB = 120;
#endif

// Default values for parameters to determine the global state.
const int kDefaultNewRenderersUntilThrottled = 4;
const int kDefaultNewRenderersUntilSuspended = 2;
const int kDefaultNewRenderersBackToNormal = 5;
const int kDefaultNewRenderersBackToThrottled = 3;
const int kDefaultMinimumTransitionPeriodSeconds = 30;
const int kDefaultMonitoringIntervalSeconds = 5;

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

// SingletonTraits for MemoryCoordinator. Returns MemoryCoordinatorImpl
// as an actual instance.
struct MemoryCoordinatorSingletonTraits
    : public base::LeakySingletonTraits<MemoryCoordinator> {
  static MemoryCoordinator* New() {
    return new MemoryCoordinatorImpl(base::ThreadTaskRunnerHandle::Get(),
                                     CreateMemoryMonitor());
  }
};

// static
MemoryCoordinator* MemoryCoordinator::GetInstance() {
  if (!base::FeatureList::IsEnabled(features::kMemoryCoordinator))
    return nullptr;
  return base::Singleton<MemoryCoordinator,
                         MemoryCoordinatorSingletonTraits>::get();
}

MemoryCoordinatorImpl::MemoryCoordinatorImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<MemoryMonitor> memory_monitor)
    : task_runner_(task_runner),
      memory_monitor_(std::move(memory_monitor)),
      weak_ptr_factory_(this) {
  DCHECK(memory_monitor_.get());
  update_state_callback_ = base::Bind(&MemoryCoordinatorImpl::UpdateState,
                                      weak_ptr_factory_.GetWeakPtr());

  // Set initial parameters for calculating the global state.
  expected_renderer_size_ = kDefaultExpectedRendererSizeMB;
  new_renderers_until_throttled_ = kDefaultNewRenderersUntilThrottled;
  new_renderers_until_suspended_ = kDefaultNewRenderersUntilSuspended;
  new_renderers_back_to_normal_ = kDefaultNewRenderersBackToNormal;
  new_renderers_back_to_throttled_ = kDefaultNewRenderersBackToThrottled;
  minimum_transition_period_ =
      base::TimeDelta::FromSeconds(kDefaultMinimumTransitionPeriodSeconds);
  monitoring_interval_ =
      base::TimeDelta::FromSeconds(kDefaultMonitoringIntervalSeconds);
}

MemoryCoordinatorImpl::~MemoryCoordinatorImpl() {}

void MemoryCoordinatorImpl::Start() {
  DCHECK(CalledOnValidThread());
  DCHECK(last_state_change_.is_null());
  DCHECK(ValidateParameters());

  notification_registrar_.Add(
      this, NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      NotificationService::AllBrowserContextsAndSources());
  ScheduleUpdateState(base::TimeDelta());
}

void MemoryCoordinatorImpl::OnChildAdded(int render_process_id) {
  // Populate the global state as an initial state of a newly created process.
  SetMemoryState(render_process_id, ToMojomMemoryState(current_state_));
}

base::MemoryState MemoryCoordinatorImpl::GetCurrentMemoryState() const {
  return current_state_;
}

void MemoryCoordinatorImpl::SetCurrentMemoryStateForTesting(
    base::MemoryState memory_state) {
  // This changes the current state temporariy for testing. The state will be
  // updated later by the task posted at ScheduleUpdateState.
  base::MemoryState prev_state = current_state_;
  current_state_ = memory_state;
  if (prev_state != current_state_) {
    NotifyStateToClients();
    NotifyStateToChildren();
  }
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
  bool is_visible = *Details<bool>(details).ptr();
  // We don't throttle/suspend a visible renderer for now.
  auto new_state = is_visible ? mojom::MemoryState::NORMAL
                              : ToMojomMemoryState(current_state_);
  SetMemoryState(iter->first, new_state);
}

base::MemoryState MemoryCoordinatorImpl::CalculateNextState() {
  using MemoryState = base::MemoryState;

  int available = memory_monitor_->GetFreeMemoryUntilCriticalMB();
  if (available <= 0)
    return MemoryState::SUSPENDED;

  int expected_renderer_count = available / expected_renderer_size_;

  switch (current_state_) {
    case MemoryState::NORMAL:
      if (expected_renderer_count <= new_renderers_until_suspended_)
        return MemoryState::SUSPENDED;
      if (expected_renderer_count <= new_renderers_until_throttled_)
        return MemoryState::THROTTLED;
      return MemoryState::NORMAL;
    case MemoryState::THROTTLED:
      if (expected_renderer_count <= new_renderers_until_suspended_)
        return MemoryState::SUSPENDED;
      if (expected_renderer_count >= new_renderers_back_to_normal_)
        return MemoryState::NORMAL;
      return MemoryState::THROTTLED;
    case MemoryState::SUSPENDED:
      if (expected_renderer_count >= new_renderers_back_to_normal_)
        return MemoryState::NORMAL;
      if (expected_renderer_count >= new_renderers_back_to_throttled_)
        return MemoryState::THROTTLED;
      return MemoryState::SUSPENDED;
    case MemoryState::UNKNOWN:
      // Fall through
    default:
      NOTREACHED();
      return MemoryState::UNKNOWN;
  }
}

void MemoryCoordinatorImpl::UpdateState() {
  base::TimeTicks now = base::TimeTicks::Now();
  MemoryState prev_state = current_state_;
  MemoryState next_state = CalculateNextState();

  if (last_state_change_.is_null() || current_state_ != next_state) {
    current_state_ = next_state;
    last_state_change_ = now;
  }

  if (next_state != prev_state) {
    NotifyStateToClients();
    NotifyStateToChildren();
    ScheduleUpdateState(minimum_transition_period_);
  } else {
    ScheduleUpdateState(monitoring_interval_);
  }
}

void MemoryCoordinatorImpl::NotifyStateToClients() {
  // SUSPENDED state may not make sense to the browser process. Use THROTTLED
  // instead when the global state is SUSPENDED.
  // TODO(bashi): Maybe worth considering another state for the browser.
  auto state = current_state_ == MemoryState::SUSPENDED ? MemoryState::THROTTLED
                                                        : current_state_;
  base::MemoryCoordinatorClientRegistry::GetInstance()->Notify(state);
}

void MemoryCoordinatorImpl::NotifyStateToChildren() {
  auto mojo_state = ToMojomMemoryState(current_state_);
  // It's OK to call SetMemoryState() unconditionally because it checks whether
  // this state transition is valid.
  for (auto& iter : children())
    SetMemoryState(iter.first, mojo_state);
}

void MemoryCoordinatorImpl::ScheduleUpdateState(base::TimeDelta delta) {
  task_runner_->PostDelayedTask(FROM_HERE, update_state_callback_, delta);
}

bool MemoryCoordinatorImpl::ValidateParameters() {
  return (new_renderers_until_throttled_ > new_renderers_until_suspended_) &&
      (new_renderers_back_to_normal_ > new_renderers_back_to_throttled_) &&
      (new_renderers_back_to_normal_ > new_renderers_until_throttled_) &&
      (new_renderers_back_to_throttled_ > new_renderers_until_suspended_);
}

}  // namespace content
