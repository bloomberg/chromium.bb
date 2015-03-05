// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WORKER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WORKER_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler.h"

namespace content {
namespace devtools {
namespace worker {

class WorkerHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  WorkerHandler();
  virtual ~WorkerHandler();

  void SetClient(scoped_ptr<DevToolsProtocolClient> client);

  Response DisconnectFromWorker(int worker_id);

  Response DisconnectFromWorker(const std::string& worker_id);

 private:
  scoped_ptr<DevToolsProtocolClient> client_;

  DISALLOW_COPY_AND_ASSIGN(WorkerHandler);
};

}  // namespace worker
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WORKER_HANDLER_H_
