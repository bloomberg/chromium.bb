// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/protocol/devtools_protocol_handler.h"
#include "content/public/browser/tracing_controller.h"

namespace base {
class RefCountedString;
class Timer;
}

namespace content {
namespace devtools {
namespace tracing {

class TracingHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  enum Target { Browser, Renderer };
  explicit TracingHandler(Target target);
  virtual ~TracingHandler();

  void SetClient(scoped_ptr<Client> client);
  void Detached();

  void OnTraceDataCollected(const std::string& trace_fragment);
  void OnTraceComplete();

  Response Start(DevToolsCommandId command_id,
                 const std::string* categories,
                 const std::string* options,
                 const double* buffer_usage_reporting_interval);
  Response End(DevToolsCommandId command_id);
  Response GetCategories(DevToolsCommandId command);

 private:
  void OnRecordingEnabled(DevToolsCommandId command_id);
  void OnBufferUsage(float percent_full, size_t approximate_event_count);

  void OnCategoriesReceived(DevToolsCommandId command_id,
                            const std::set<std::string>& category_set);

  base::trace_event::TraceOptions TraceOptionsFromString(
      const std::string* options);

  void SetupTimer(double usage_reporting_interval);

  void DisableRecording(bool abort);

  scoped_ptr<base::Timer> buffer_usage_poll_timer_;
  Target target_;
  bool is_recording_;

  scoped_ptr<Client> client_;
  base::WeakPtrFactory<TracingHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TracingHandler);
};

}  // namespace tracing
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
