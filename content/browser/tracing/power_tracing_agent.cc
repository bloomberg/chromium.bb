// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/trace_event/trace_event_impl.h"
#include "content/browser/tracing/battor_power_trace_provider.h"
#include "content/browser/tracing/power_tracing_agent.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

const char kPowerTracingAgentName[] = "battor";
const char kPowerTraceLabel[] = "powerTraceAsString";

}  // namespace

// static
PowerTracingAgent* PowerTracingAgent::GetInstance() {
  return base::Singleton<PowerTracingAgent>::get();
}

PowerTracingAgent::PowerTracingAgent() : is_tracing_(false) {
  battor_trace_provider_.reset(new BattorPowerTraceProvider());
}

PowerTracingAgent::~PowerTracingAgent() {}

std::string PowerTracingAgent::GetTracingAgentName() {
  return kPowerTracingAgentName;
}

std::string PowerTracingAgent::GetTraceEventLabel() {
  return kPowerTraceLabel;
}

bool PowerTracingAgent::StartAgentTracing(
    const base::trace_event::TraceConfig& trace_config) {
  // Tracing session already in progress.
  if (is_tracing_)
    return false;

  // TODO(prabhur) Start tracing probably needs to be done in a
  // separate thread since it involves talking to the h/w.
  // Revisit once battor h/w communication is enabled.
  is_tracing_ = battor_trace_provider_->StartTracing();
  return is_tracing_;
}

void PowerTracingAgent::StopAgentTracing(
    const StopAgentTracingCallback& callback) {
  // No tracing session in progress.
  if (!is_tracing_)
    return;

  // Stop tracing & collect logs.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PowerTracingAgent::FlushOnThread,
                 base::Unretained(this),
                 callback));
}

void PowerTracingAgent::OnStopTracingDone(
    const StopAgentTracingCallback& callback,
    const scoped_refptr<base::RefCountedString>& result) {
  is_tracing_ = false;
  // Pass the serialized events.
  callback.Run(GetTracingAgentName(), GetTraceEventLabel(), result);
}

void PowerTracingAgent::FlushOnThread(
    const StopAgentTracingCallback& callback) {
  // Pass the result to the UI Thread.

  // TODO(prabhur) StopTracing & GetLog need to be called on a
  // separate thread depending on how BattorPowerTraceProvider is implemented.
  battor_trace_provider_->StopTracing();
  std::string battor_logs;
  battor_trace_provider_->GetLog(&battor_logs);
  scoped_refptr<base::RefCountedString> result =
      base::RefCountedString::TakeString(&battor_logs);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PowerTracingAgent::OnStopTracingDone,
                 base::Unretained(this),
                 callback,
                 result));
}

bool PowerTracingAgent::SupportsExplicitClockSync() {
  // TODO(zhenw): return true after implementing explicit clock sync.
  return false;
}

void PowerTracingAgent::RecordClockSyncMarker(
    int sync_id,
    const RecordClockSyncMarkerCallback& callback) {
  DCHECK(SupportsExplicitClockSync());
  // TODO(zhenw): implement explicit clock sync.
}

}  // namespace content
