// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler_impl.h"

namespace content {
namespace devtools {
namespace tracing {

class TracingHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  TracingHandler();
  virtual ~TracingHandler();

  void SetClient(scoped_ptr<Client> client);

  Response Start(const std::string& categories,
                 const std::string& options,
                 const double* buffer_usage_reporting_interval);

  Response End();

  scoped_refptr<DevToolsProtocol::Response> GetCategories(
      scoped_refptr<DevToolsProtocol::Command> command);

 private:
  scoped_ptr<Client> client_;

  DISALLOW_COPY_AND_ASSIGN(TracingHandler);
};

}  // namespace tracing
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
