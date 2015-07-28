// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
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
  Response RequestMemoryDump(DevToolsCommandId command_id);
  bool did_initiate_recording() { return did_initiate_recording_; }

 private:
  void OnRecordingEnabled(DevToolsCommandId command_id);
  void OnBufferUsage(float percent_full, size_t approximate_event_count);

  void OnCategoriesReceived(DevToolsCommandId command_id,
                            const std::set<std::string>& category_set);
  void OnMemoryDumpFinished(DevToolsCommandId command_id,
                            uint64 dump_guid,
                            bool success);

  void SetupTimer(double usage_reporting_interval);

  void DisableRecording(bool abort);

  bool IsRecording() const;

  scoped_ptr<base::Timer> buffer_usage_poll_timer_;
  Target target_;

  scoped_ptr<Client> client_;
  bool did_initiate_recording_;
  base::WeakPtrFactory<TracingHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TracingHandler);
};

}  // namespace tracing
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
