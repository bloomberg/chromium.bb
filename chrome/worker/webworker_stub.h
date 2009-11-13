// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WEBWORKER_STUB_H_
#define CHROME_WORKER_WEBWORKER_STUB_H_

#include "chrome/worker/webworker_stub_base.h"
#include "chrome/worker/webworkerclient_proxy.h"
#include "googleurl/src/gurl.h"

namespace WebKit {
class WebWorker;
}

// This class creates a WebWorker, and translates incoming IPCs to the
// appropriate WebWorker APIs.
class WebWorkerStub : public WebWorkerStubBase {
 public:
  WebWorkerStub(const GURL& url, int route_id);

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);

 private:
  virtual ~WebWorkerStub();

  void OnTerminateWorkerContext();
  void OnPostMessage(const string16& message,
                     const std::vector<int>& sent_message_port_ids,
                     const std::vector<int>& new_routing_ids);

  WebKit::WebWorker* impl_;

  DISALLOW_COPY_AND_ASSIGN(WebWorkerStub);
};

#endif  // CHROME_WORKER_WEBWORKER_STUB_H_
