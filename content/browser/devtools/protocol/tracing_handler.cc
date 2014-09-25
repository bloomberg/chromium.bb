// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/tracing_handler.h"

namespace content {
namespace devtools {
namespace tracing {

typedef DevToolsProtocolClient::Response Response;

TracingHandler::TracingHandler() {
}

TracingHandler::~TracingHandler() {
}

void TracingHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

Response TracingHandler::Start(const std::string& categories,
                               const std::string& options,
                               const double* buffer_usage_reporting_interval) {
  return Response::FallThrough();
}

Response TracingHandler::End() {
  return Response::FallThrough();
}

scoped_refptr<DevToolsProtocol::Response> TracingHandler::GetCategories(
    scoped_refptr<DevToolsProtocol::Command> command) {
  return NULL;
}

}  // namespace tracing
}  // namespace devtools
}  // namespace content
