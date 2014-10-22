// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_

#include <set>
#include <string>

#include "base/debug/trace_event.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_protocol_handler_impl.h"
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

  scoped_refptr<DevToolsProtocol::Response> Start(
      const std::string* categories,
      const std::string* options,
      const double* buffer_usage_reporting_interval,
      scoped_refptr<DevToolsProtocol::Command> command);

  scoped_refptr<DevToolsProtocol::Response> End(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> GetCategories(
      scoped_refptr<DevToolsProtocol::Command> command);

 private:
  void OnRecordingEnabled(scoped_refptr<DevToolsProtocol::Command> command);
  void OnBufferUsage(float usage);

  void OnCategoriesReceived(scoped_refptr<DevToolsProtocol::Command> command,
                            const std::set<std::string>& category_set);

  base::debug::TraceOptions TraceOptionsFromString(const std::string* options);

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
