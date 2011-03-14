// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WEBSHAREDWORKER_STUB_H_
#define CONTENT_WORKER_WEBSHAREDWORKER_STUB_H_
#pragma once

#include "content/worker/webworker_stub_base.h"
#include "content/worker/webworkerclient_proxy.h"
#include "googleurl/src/gurl.h"

namespace WebKit {
class WebSharedWorker;
}

// This class creates a WebSharedWorker, and translates incoming IPCs to the
// appropriate WebSharedWorker APIs.
class WebSharedWorkerStub : public WebWorkerStubBase {
 public:
  WebSharedWorkerStub(const string16& name, int route_id,
                      const WorkerAppCacheInitInfo& appcache_init_info);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelError();

  virtual const GURL& url() const;

 private:
  virtual ~WebSharedWorkerStub();

  void OnConnect(int sent_message_port_id, int routing_id);
  void OnStartWorkerContext(
      const GURL& url, const string16& user_agent, const string16& source_code);
  void OnTerminateWorkerContext();

  WebKit::WebSharedWorker* impl_;
  string16 name_;
  bool started_;
  GURL url_;

  typedef std::pair<int, int> PendingConnectInfo;
  typedef std::vector<PendingConnectInfo> PendingConnectInfoList;
  PendingConnectInfoList pending_connects_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerStub);
};

#endif  // CONTENT_WORKER_WEBSHAREDWORKER_STUB_H_
