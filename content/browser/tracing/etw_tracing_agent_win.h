// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_ETW_TRACING_AGENT_H_
#define CONTENT_BROWSER_TRACING_ETW_TRACING_AGENT_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "base/win/event_trace_consumer.h"
#include "base/win/event_trace_controller.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/interfaces/tracing/tracing.mojom.h"

namespace service_manager {
class Connector;
}

namespace content {

class EtwTracingAgent : public base::win::EtwTraceConsumerBase<EtwTracingAgent>,
                        public tracing::mojom::Agent {
 public:
  explicit EtwTracingAgent(service_manager::Connector* connector);

  // Retrieve the ETW consumer instance.
  static EtwTracingAgent* GetInstance();

 private:
  friend std::default_delete<EtwTracingAgent>;

  ~EtwTracingAgent() override;

  void AddSyncEventToBuffer();
  void AppendEventToBuffer(EVENT_TRACE* event);

  // tracing::mojom::Agent. Called by Mojo internals on the UI thread.
  void StartTracing(const std::string& config,
                    base::TimeTicks coordinator_time,
                    const Agent::StartTracingCallback& callback) override;
  void StopAndFlush(tracing::mojom::RecorderPtr recorder) override;
  void RequestClockSyncMarker(
      const std::string& sync_id,
      const Agent::RequestClockSyncMarkerCallback& callback) override;
  void GetCategories(const Agent::GetCategoriesCallback& callback) override;
  void RequestBufferStatus(
      const Agent::RequestBufferStatusCallback& callback) override;

  // Static override of EtwTraceConsumerBase::ProcessEvent.
  // @param event the raw ETW event to process.
  friend class base::win::EtwTraceConsumerBase<EtwTracingAgent>;
  static void ProcessEvent(EVENT_TRACE* event);

  // Request the ETW trace controller to activate the kernel tracing.
  // returns true on success, false if the kernel tracing isn't activated.
  bool StartKernelSessionTracing();

  // Request the ETW trace controller to deactivate the kernel tracing.
  // @param callback the callback to call with the consumed events.
  // @returns true on success, false if an error occurred.
  bool StopKernelSessionTracing();

  void OnStopSystemTracingDone(const std::string& output);
  void TraceAndConsumeOnThread();
  void FlushOnThread();

  mojo::Binding<tracing::mojom::Agent> binding_;
  std::unique_ptr<base::ListValue> events_;
  base::Thread thread_;
  TRACEHANDLE session_handle_;
  base::win::EtwTraceProperties properties_;
  tracing::mojom::RecorderPtr recorder_;
  bool is_tracing_;

  DISALLOW_COPY_AND_ASSIGN(EtwTracingAgent);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_ETW_TRACING_AGENT_H_
