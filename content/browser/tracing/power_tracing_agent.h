// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_POWER_TRACING_AGENT_H_
#define CONTENT_BROWSER_TRACING_POWER_TRACING_AGENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread.h"
#include "base/trace_event/tracing_agent.h"

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {

class BattorPowerTraceProvider;

class PowerTracingAgent : public base::trace_event::TracingAgent {
 public:
  // Retrieve the singleton instance.
  static PowerTracingAgent* GetInstance();

  // base::trace_event::TracingAgent implementation.
  std::string GetTracingAgentName() override;
  std::string GetTraceEventLabel() override;

  void StartAgentTracing(const base::trace_event::TraceConfig& trace_config,
                         const StartAgentTracingCallback& callback) override;
  void StopAgentTracing(const StopAgentTracingCallback& callback) override;

  bool SupportsExplicitClockSync() override;
  void RecordClockSyncMarker(
      int sync_id,
      const RecordClockSyncMarkerCallback& callback) override;

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct base::DefaultSingletonTraits<PowerTracingAgent>;

  // Constructor.
  PowerTracingAgent();
  ~PowerTracingAgent() override;

  void OnStopTracingDone(const StopAgentTracingCallback& callback,
                         const scoped_refptr<base::RefCountedString>& result);

  void TraceOnThread();
  void FlushOnThread(const StopAgentTracingCallback& callback);
  void RecordClockSyncMarkerOnThread(
      int sync_id,
      const RecordClockSyncMarkerCallback& callback);

  base::Thread thread_;
  scoped_ptr<BattorPowerTraceProvider> battor_trace_provider_;

  DISALLOW_COPY_AND_ASSIGN(PowerTracingAgent);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_POWER_TRACING_AGENT_H_
