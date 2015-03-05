// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/worker_handler.h"

namespace content {
namespace devtools {
namespace worker {

typedef DevToolsProtocolClient::Response Response;

WorkerHandler::WorkerHandler() {
}

WorkerHandler::~WorkerHandler() {
}

void WorkerHandler::SetClient(scoped_ptr<DevToolsProtocolClient> client) {
}

Response WorkerHandler::DisconnectFromWorker(int worker_id) {
  return Response::FallThrough();
}

Response WorkerHandler::DisconnectFromWorker(const std::string& worker_id) {
  return Response::FallThrough();
}

}  // namespace worker
}  // namespace devtools
}  // namespace content
