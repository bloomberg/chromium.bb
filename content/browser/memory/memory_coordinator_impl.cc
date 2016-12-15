// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator_impl.h"

#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/memory/memory_monitor.h"
#include "content/browser/memory/memory_state_updater.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_features.h"

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

void RecordMetricsOnStateChange(base::MemoryState prev_state,
                                base::MemoryState next_state,
                                base::TimeDelta duration,
                                size_t total_private_mb) {
#define RECORD_METRICS(transition)                                             \
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Coordinator.TotalPrivate." transition, \
                                total_private_mb);                             \
  UMA_HISTOGRAM_CUSTOM_TIMES("Memory.Coordinator.StateDuration." transition,   \
                             duration, base::TimeDelta::FromSeconds(30),       \
                             base::TimeDelta::FromHours(24), 50);

  if (prev_state == base::MemoryState::NORMAL) {
    switch (next_state) {
      case base::MemoryState::THROTTLED:
        RECORD_METRICS("NormalToThrottled");
        break;
      case base::MemoryState::SUSPENDED:
        RECORD_METRICS("NormalToSuspended");
        break;
      case base::MemoryState::UNKNOWN:
      case base::MemoryState::NORMAL:
        NOTREACHED();
        break;
    }
  } else if (prev_state == base::MemoryState::THROTTLED) {
    switch (next_state) {
      case base::MemoryState::NORMAL:
        RECORD_METRICS("ThrottledToNormal");
        break;
      case base::MemoryState::SUSPENDED:
        RECORD_METRICS("ThrottledToSuspended");
        break;
      case base::MemoryState::UNKNOWN:
      case base::MemoryState::THROTTLED:
        NOTREACHED();
        break;
    }
  } else if (prev_state == base::MemoryState::SUSPENDED) {
    switch (next_state) {
      case base::MemoryState::NORMAL:
        RECORD_METRICS("SuspendedToNormal");
        break;
      case base::MemoryState::THROTTLED:
        RECORD_METRICS("SuspendedToThrottled");
        break;
      case base::MemoryState::UNKNOWN:
      case base::MemoryState::SUSPENDED:
        NOTREACHED();
        break;
    }
  } else {
    NOTREACHED();
  }
#undef RECORD_METRICS
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
    : memory_monitor_(std::move(memory_monitor)),
      state_updater_(base::MakeUnique<MemoryStateUpdater>(this, task_runner)) {
  DCHECK(memory_monitor_.get());
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

void MemoryCoordinatorImpl::OnChildAdded(int render_process_id) {
  // Populate the global state as an initial state of a newly created process.
  auto new_state = ToMojomMemoryState(GetGlobalMemoryState());
  SetChildMemoryState(render_process_id, new_state);
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
  auto new_state = ToMojomMemoryState(GetGlobalMemoryState());
  SetChildMemoryState(iter->first, new_state);
}

bool MemoryCoordinatorImpl::ChangeStateIfNeeded(base::MemoryState prev_state,
                                                base::MemoryState next_state) {
  DCHECK(CalledOnValidThread());
  if (prev_state == next_state)
    return false;

  base::TimeTicks prev_last_state_change = last_state_change_;
  last_state_change_ = base::TimeTicks::Now();
  current_state_ = next_state;

  TRACE_EVENT2("memory-infra", "MemoryCoordinatorImpl::ChangeStateIfNeeded",
               "prev", MemoryStateToString(prev_state),
               "next", MemoryStateToString(next_state));
  RecordStateChange(prev_state, next_state,
                    last_state_change_ - prev_last_state_change);
  NotifyStateToClients();
  NotifyStateToChildren();
  return true;
}

void MemoryCoordinatorImpl::NotifyStateToClients() {
  auto state = GetCurrentMemoryState();
  base::MemoryCoordinatorClientRegistry::GetInstance()->Notify(state);
}

void MemoryCoordinatorImpl::NotifyStateToChildren() {
  auto mojo_state = ToMojomMemoryState(current_state_);
  // It's OK to call SetChildMemoryState() unconditionally because it checks
  // whether this state transition is valid.
  for (auto& iter : children())
    SetChildMemoryState(iter.first, mojo_state);
}

void MemoryCoordinatorImpl::RecordStateChange(MemoryState prev_state,
                                              MemoryState next_state,
                                              base::TimeDelta duration) {
  size_t total_private_kb = 0;

  // TODO(bashi): On MacOS we can't get process metrics for child processes and
  // therefore can't calculate the total private memory.
#if !defined(OS_MACOSX)
  auto browser_metrics = base::ProcessMetrics::CreateCurrentProcessMetrics();
  base::WorkingSetKBytes working_set;
  browser_metrics->GetWorkingSetKBytes(&working_set);
  total_private_kb += working_set.priv;

  for (auto& iter : children()) {
    auto* render_process_host = RenderProcessHost::FromID(iter.first);
    if (!render_process_host ||
        render_process_host->GetHandle() == base::kNullProcessHandle)
      continue;
    auto metrics = base::ProcessMetrics::CreateProcessMetrics(
        render_process_host->GetHandle());
    metrics->GetWorkingSetKBytes(&working_set);
    total_private_kb += working_set.priv;
  }
#endif

  RecordMetricsOnStateChange(prev_state, next_state, duration,
                             total_private_kb / 1024);
}

}  // namespace content
