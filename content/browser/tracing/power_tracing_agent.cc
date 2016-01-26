// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/thread_task_runner_handle.h"
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

PowerTracingAgent::PowerTracingAgent() : thread_("PowerTracingAgentThread") {
  battor_trace_provider_.reset(new BattorPowerTraceProvider());
}

PowerTracingAgent::~PowerTracingAgent() {}

std::string PowerTracingAgent::GetTracingAgentName() {
  return kPowerTracingAgentName;
}

std::string PowerTracingAgent::GetTraceEventLabel() {
  return kPowerTraceLabel;
}

void PowerTracingAgent::StartAgentTracing(
    const base::trace_event::TraceConfig& trace_config,
    const StartAgentTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(charliea): When system tracing is enabled in about://tracing, it will
  // trigger power tracing. We need a way of checking if BattOr is connected.
  // Currently, IsConnected() always returns false, so that we do not include
  // BattOr trace until it is hooked up.
  if (!battor_trace_provider_->IsConnected()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, GetTracingAgentName(), false /* success */));
    return;
  }
  thread_.Start();

  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&PowerTracingAgent::TraceOnThread, base::Unretained(this)));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, GetTracingAgentName(), true /* success */));
}

void PowerTracingAgent::StopAgentTracing(
    const StopAgentTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(thread_.IsRunning());

  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&PowerTracingAgent::FlushOnThread, base::Unretained(this),
                 callback));
}

void PowerTracingAgent::OnStopTracingDone(
    const StopAgentTracingCallback& callback,
    const scoped_refptr<base::RefCountedString>& result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Pass the serialized events.
  callback.Run(GetTracingAgentName(), GetTraceEventLabel(), result);

  // Stop the power tracing agent thread on file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&base::Thread::Stop, base::Unretained(&thread_)));
}

void PowerTracingAgent::TraceOnThread() {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());
  battor_trace_provider_->StartTracing();
}

void PowerTracingAgent::FlushOnThread(
    const StopAgentTracingCallback& callback) {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());

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
  return true;
}

void PowerTracingAgent::RecordClockSyncMarker(
    int sync_id,
    const RecordClockSyncMarkerCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(SupportsExplicitClockSync());

  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&PowerTracingAgent::RecordClockSyncMarkerOnThread,
                 base::Unretained(this),
                 sync_id,
                 callback));
}

void PowerTracingAgent::RecordClockSyncMarkerOnThread(
    int sync_id,
    const RecordClockSyncMarkerCallback& callback) {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(SupportsExplicitClockSync());

  base::TimeTicks issue_ts = base::TimeTicks::Now();
  battor_trace_provider_->RecordClockSyncMarker(sync_id);
  base::TimeTicks issue_end_ts = base::TimeTicks::Now();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, sync_id, issue_ts, issue_end_ts));
}

}  // namespace content
