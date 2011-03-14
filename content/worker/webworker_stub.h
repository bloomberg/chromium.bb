// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WEBWORKER_STUB_H_
#define CONTENT_WORKER_WEBWORKER_STUB_H_
#pragma once

#include "content/worker/webworker_stub_base.h"
#include "content/worker/webworkerclient_proxy.h"
#include "googleurl/src/gurl.h"

namespace WebKit {
class WebWorker;
}

// This class creates a WebWorker, and translates incoming IPCs to the
// appropriate WebWorker APIs.
class WebWorkerStub : public WebWorkerStubBase {
 public:
  WebWorkerStub(const GURL& url, int route_id,
                const WorkerAppCacheInitInfo& appcache_init_info);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelError();

  virtual const GURL& url() const;

 private:
  virtual ~WebWorkerStub();

  void OnTerminateWorkerContext();
  void OnPostMessage(const string16& message,
                     const std::vector<int>& sent_message_port_ids,
                     const std::vector<int>& new_routing_ids);

  WebKit::WebWorker* impl_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(WebWorkerStub);
};

#endif  // CONTENT_WORKER_WEBWORKER_STUB_H_
