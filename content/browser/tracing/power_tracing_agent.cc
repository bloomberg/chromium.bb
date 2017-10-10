// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/power_tracing_agent.h"

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "tools/battor_agent/battor_finder.h"

namespace content {

namespace {

const char kPowerTraceLabel[] = "powerTraceAsString";

}  // namespace

// static
PowerTracingAgent* PowerTracingAgent::GetInstance() {
  return base::Singleton<PowerTracingAgent>::get();
}

PowerTracingAgent::PowerTracingAgent(service_manager::Connector* connector)
    : binding_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Connect to the agent registry interface.
  tracing::mojom::AgentRegistryPtr agent_registry;
  connector->BindInterface(resource_coordinator::mojom::kServiceName,
                           &agent_registry);

  // Register this agent.
  tracing::mojom::AgentPtr agent;
  binding_.Bind(mojo::MakeRequest(&agent));
  agent_registry->RegisterAgent(std::move(agent), kPowerTraceLabel,
                                tracing::mojom::TraceDataType::STRING,
                                true /* supports_explicit_clock_sync */);
}

PowerTracingAgent::PowerTracingAgent() : binding_(this) {}
PowerTracingAgent::~PowerTracingAgent() = default;

void PowerTracingAgent::StartTracing(
    const std::string& config,
    base::TimeTicks coordinator_time,
    const Agent::StartTracingCallback& callback) {
  base::trace_event::TraceConfig trace_config(config);
  if (!trace_config.IsSystraceEnabled()) {
    callback.Run(false /* success */);
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BACKGROUND, base::MayBlock()},
      base::BindOnce(&PowerTracingAgent::FindBattOrOnBackgroundThread,
                     base::Unretained(this), callback));
}

void PowerTracingAgent::FindBattOrOnBackgroundThread(
    const Agent::StartTracingCallback& callback) {
  std::string path = battor::BattOrFinder::FindBattOr();
  if (path.empty()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(callback, false /* success */));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&PowerTracingAgent::StartTracingOnIOThread,
                     base::Unretained(this), path, callback));
}

void PowerTracingAgent::StartTracingOnIOThread(
    const std::string& path,
    const Agent::StartTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  battor_agent_.reset(new battor::BattOrAgent(
      path, this, BrowserThread::GetTaskRunnerForThread(BrowserThread::UI)));

  start_tracing_callback_ = callback;
  battor_agent_->StartTracing();
}

void PowerTracingAgent::OnStartTracingComplete(battor::BattOrError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bool success = (error == battor::BATTOR_ERROR_NONE);
  if (!success)
    battor_agent_.reset();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(start_tracing_callback_, success));
  start_tracing_callback_.Reset();
}

void PowerTracingAgent::StopAndFlush(tracing::mojom::RecorderPtr recorder) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&PowerTracingAgent::StopAndFlushOnIOThread,
                     base::Unretained(this), std::move(recorder)));
}

void PowerTracingAgent::StopAndFlushOnIOThread(
    tracing::mojom::RecorderPtr recorder) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // This makes sense only when the battor agent exists.
  if (battor_agent_) {
    recorder_ = std::move(recorder);
    battor_agent_->StopTracing();
  }
}

void PowerTracingAgent::OnStopTracingComplete(
    const battor::BattOrResults& results,
    battor::BattOrError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error == battor::BATTOR_ERROR_NONE)
    recorder_->AddChunk(results.ToString());
  recorder_.reset();
  battor_agent_.reset();
}

void PowerTracingAgent::RequestClockSyncMarker(
    const std::string& sync_id,
    const Agent::RequestClockSyncMarkerCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&PowerTracingAgent::RequestClockSyncMarkerOnIOThread,
                     base::Unretained(this), sync_id, callback));
}

void PowerTracingAgent::RequestClockSyncMarkerOnIOThread(
    const std::string& sync_id,
    const Agent::RequestClockSyncMarkerCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // This makes sense only when the battor agent exists.
  if (!battor_agent_) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(callback, base::TimeTicks(), base::TimeTicks()));
    return;
  }

  request_clock_sync_marker_callback_ = callback;
  request_clock_sync_marker_start_time_ = base::TimeTicks::Now();
  battor_agent_->RecordClockSyncMarker(sync_id);
}

void PowerTracingAgent::OnRecordClockSyncMarkerComplete(
    battor::BattOrError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::TimeTicks issue_start_ts = request_clock_sync_marker_start_time_;
  base::TimeTicks issue_end_ts = base::TimeTicks::Now();

  if (error != battor::BATTOR_ERROR_NONE)
    issue_start_ts = issue_end_ts = base::TimeTicks();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(request_clock_sync_marker_callback_,
                                         issue_start_ts, issue_end_ts));

  request_clock_sync_marker_callback_.Reset();
  request_clock_sync_marker_start_time_ = base::TimeTicks();
}

void PowerTracingAgent::OnGetFirmwareGitHashComplete(
    const std::string& version, battor::BattOrError error) {
  return;
}

void PowerTracingAgent::GetCategories(
    const Agent::GetCategoriesCallback& callback) {
  callback.Run("");
}

void PowerTracingAgent::RequestBufferStatus(
    const Agent::RequestBufferStatusCallback& callback) {
  callback.Run(0, 0);
}

}  // namespace content
