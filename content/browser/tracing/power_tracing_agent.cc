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

PowerTracingAgent::PowerTracingAgent() : is_tracing_(false) {
  battor_trace_provider_.reset(new BattorPowerTraceProvider());
}

PowerTracingAgent::~PowerTracingAgent() {}

bool PowerTracingAgent::StartTracing() {
  // Tracing session already in progress.
  if (is_tracing_)
    return false;

  // TODO(prabhur) Start tracing probably needs to be done in a
  // separate thread since it involves talking to the h/w.
  // Revisit once battor h/w communication is enabled.
  is_tracing_ = battor_trace_provider_->StartTracing();
  return is_tracing_;
}

void PowerTracingAgent::StopTracing(const OutputCallback& callback) {
  // No tracing session in progress.
  if (!is_tracing_)
    return;

  // Stop tracing & collect logs.
  OutputCallback on_stop_power_tracing_done_callback = base::Bind(
      &PowerTracingAgent::OnStopTracingDone, base::Unretained(this), callback);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PowerTracingAgent::FlushOnThread, base::Unretained(this),
                 on_stop_power_tracing_done_callback));
}

void PowerTracingAgent::OnStopTracingDone(
    const OutputCallback& callback,
    const scoped_refptr<base::RefCountedString>& result) {
  is_tracing_ = false;
  // Pass the serialized events.
  callback.Run(result);
}

// static
PowerTracingAgent* PowerTracingAgent::GetInstance() {
  return base::Singleton<PowerTracingAgent>::get();
}

void PowerTracingAgent::FlushOnThread(const OutputCallback& callback) {
  // Pass the result to the UI Thread.

  // TODO(prabhur) StopTracing & GetLog need to be called on a
  // separate thread depending on how BattorPowerTraceProvider is implemented.
  battor_trace_provider_->StopTracing();
  std::string battor_logs;
  battor_trace_provider_->GetLog(&battor_logs);
  scoped_refptr<base::RefCountedString> result =
      base::RefCountedString::TakeString(&battor_logs);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, result));
}

}  // namespace content
