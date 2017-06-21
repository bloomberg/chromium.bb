// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/tracing.h"
#include "content/common/content_export.h"
#include "content/public/browser/tracing_controller.h"

namespace base {
class Timer;
}

namespace content {

class DevToolsAgentHostImpl;
class DevToolsIOContext;

namespace protocol {

class TracingHandler : public DevToolsDomainHandler,
                       public Tracing::Backend {
 public:
  enum Target { Browser, Renderer };
  TracingHandler(Target target,
                 int frame_tree_node_id,
                 DevToolsIOContext* io_context);
  ~TracingHandler() override;

  static std::vector<TracingHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  void OnTraceDataCollected(const std::string& trace_fragment);
  void OnTraceComplete();
  void OnTraceToStreamComplete(const std::string& stream_handle);

  // Protocol methods.
  void Start(Maybe<std::string> categories,
             Maybe<std::string> options,
             Maybe<double> buffer_usage_reporting_interval,
             Maybe<std::string> transfer_mode,
             Maybe<Tracing::TraceConfig> config,
             std::unique_ptr<StartCallback> callback) override;
  void End(std::unique_ptr<EndCallback> callback) override;
  void GetCategories(std::unique_ptr<GetCategoriesCallback> callback) override;
  void RequestMemoryDump(
      std::unique_ptr<RequestMemoryDumpCallback> callback) override;
  Response RecordClockSyncMarker(const std::string& sync_id) override;

  bool did_initiate_recording() { return did_initiate_recording_; }

 private:
  void OnRecordingEnabled(std::unique_ptr<StartCallback> callback);
  void OnBufferUsage(float percent_full, size_t approximate_event_count);
  void OnCategoriesReceived(std::unique_ptr<GetCategoriesCallback> callback,
                            const std::set<std::string>& category_set);
  void OnMemoryDumpFinished(std::unique_ptr<RequestMemoryDumpCallback> callback,
                            bool success,
                            uint64_t dump_id);

  void SetupTimer(double usage_reporting_interval);
  void StopTracing(
      const scoped_refptr<TracingController::TraceDataSink>& trace_data_sink);
  bool IsTracing() const;
  static bool IsStartupTracingActive();
  CONTENT_EXPORT static base::trace_event::TraceConfig
      GetTraceConfigFromDevToolsConfig(
          const base::DictionaryValue& devtools_config);

  std::unique_ptr<base::Timer> buffer_usage_poll_timer_;
  Target target_;

  std::unique_ptr<Tracing::Frontend> frontend_;
  DevToolsIOContext* io_context_;
  int frame_tree_node_id_;
  bool did_initiate_recording_;
  bool return_as_stream_;
  base::WeakPtrFactory<TracingHandler> weak_factory_;

  FRIEND_TEST_ALL_PREFIXES(TracingHandlerTest,
                           GetTraceConfigFromDevToolsConfig);
  DISALLOW_COPY_AND_ASSIGN(TracingHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
