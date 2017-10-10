// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/cros_tracing_agent.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task_scheduler/post_task.h"
#include "base/trace_event/trace_config.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {
const char kCrOSTraceLabel[] = "systemTraceEvents";
}  // namespace

CrOSTracingAgent::CrOSTracingAgent(service_manager::Connector* connector)
    : binding_(this) {
  // Connecto to the agent registry interface.
  tracing::mojom::AgentRegistryPtr agent_registry;
  connector->BindInterface(resource_coordinator::mojom::kServiceName,
                           &agent_registry);

  // Register this agent.
  tracing::mojom::AgentPtr agent;
  binding_.Bind(mojo::MakeRequest(&agent));
  agent_registry->RegisterAgent(std::move(agent), kCrOSTraceLabel,
                                tracing::mojom::TraceDataType::STRING,
                                false /* supports_explicit_clock_sync */);
}

CrOSTracingAgent::~CrOSTracingAgent() = default;

// tracing::mojom::Agent. Called by Mojo internals on the UI thread.
void CrOSTracingAgent::StartTracing(
    const std::string& config,
    base::TimeTicks coordinator_time,
    const Agent::StartTracingCallback& callback) {
  base::trace_event::TraceConfig trace_config(config);
  debug_daemon_ = chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  if (!trace_config.IsSystraceEnabled() || !debug_daemon_) {
    callback.Run(false /* success */);
    return;
  }
  start_tracing_callback_ = std::move(callback);
  debug_daemon_->SetStopAgentTracingTaskRunner(
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()}));
  debug_daemon_->StartAgentTracing(
      trace_config,
      base::BindRepeating(&CrOSTracingAgent::StartTracingCallbackProxy,
                          base::Unretained(this)));
}

void CrOSTracingAgent::StopAndFlush(tracing::mojom::RecorderPtr recorder) {
  if (!debug_daemon_)
    return;
  recorder_ = std::move(recorder);
  debug_daemon_->StopAgentTracing(base::BindRepeating(
      &CrOSTracingAgent::RecorderProxy, base::Unretained(this)));
}

void CrOSTracingAgent::RequestClockSyncMarker(
    const std::string& sync_id,
    const Agent::RequestClockSyncMarkerCallback& callback) {
  NOTREACHED();
}

void CrOSTracingAgent::GetCategories(
    const Agent::GetCategoriesCallback& callback) {
  callback.Run("" /* categories */);
}

void CrOSTracingAgent::RequestBufferStatus(
    const Agent::RequestBufferStatusCallback& callback) {
  callback.Run(0 /* capacity */, 0 /* count */);
}

void CrOSTracingAgent::StartTracingCallbackProxy(const std::string& agent_name,
                                                 bool success) {
  start_tracing_callback_.Run(success);
}

void CrOSTracingAgent::RecorderProxy(
    const std::string& event_name,
    const std::string& events_label,
    const scoped_refptr<base::RefCountedString>& events) {
  if (events && !events->data().empty())
    recorder_->AddChunk(events->data());
  recorder_.reset();
}

}  // namespace content
