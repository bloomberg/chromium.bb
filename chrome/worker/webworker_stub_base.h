// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WEB_WORKER_STUB_BASE_H_
#define CHROME_WORKER_WEB_WORKER_STUB_BASE_H_

#include "chrome/worker/webworkerclient_proxy.h"
#include "ipc/ipc_channel.h"

// This class is the common base class for both WebWorkerStub and
// WebSharedWorkerStub and contains common setup/teardown functionality.
class WebWorkerStubBase : public IPC::Channel::Listener {
 public:
  WebWorkerStubBase(int route_id);
  virtual ~WebWorkerStubBase();

  // Invoked when the WebWorkerClientProxy is shutting down.
  void Shutdown();

  // Called after terminating the worker context to make sure that the worker
  // actually terminates (is not stuck in an infinite loop).
  void EnsureWorkerContextTerminates();

  WebWorkerClientProxy* client() { return &client_; }

 private:
  int route_id_;

  // WebWorkerClient that responds to outgoing API calls from the worker object.
  WebWorkerClientProxy client_;

  DISALLOW_COPY_AND_ASSIGN(WebWorkerStubBase);
};

#endif  // CHROME_WORKER_WEB_WORKER_STUB_BASE_H_
