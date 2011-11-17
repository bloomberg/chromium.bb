// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WEBSHAREDWORKER_STUB_H_
#define CONTENT_WORKER_WEBSHAREDWORKER_STUB_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "content/worker/webworkerclient_proxy.h"
#include "content/worker/worker_webapplicationcachehost_impl.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel.h"

namespace WebKit {
class WebSharedWorker;
}

class SharedWorkerDevToolsAgent;

// This class creates a WebSharedWorker, and translates incoming IPCs to the
// appropriate WebSharedWorker APIs.
class WebSharedWorkerStub : public IPC::Channel::Listener {
 public:
  WebSharedWorkerStub(const string16& name, int route_id,
                      const WorkerAppCacheInitInfo& appcache_init_info);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Invoked when the WebWorkerClientProxy is shutting down.
  void Shutdown();

  // Called after terminating the worker context to make sure that the worker
  // actually terminates (is not stuck in an infinite loop).
  void EnsureWorkerContextTerminates();

  WebWorkerClientProxy* client() { return &client_; }

  const WorkerAppCacheInitInfo& appcache_init_info() const {
    return appcache_init_info_;
  }

  // Returns the script url of this worker.
  const GURL& url();


 private:
  virtual ~WebSharedWorkerStub();

  void OnConnect(int sent_message_port_id, int routing_id);
  void OnStartWorkerContext(
      const GURL& url, const string16& user_agent, const string16& source_code);
  void OnTerminateWorkerContext();

  int route_id_;
  WorkerAppCacheInitInfo appcache_init_info_;

  // WebWorkerClient that responds to outgoing API calls from the worker object.
  WebWorkerClientProxy client_;

  WebKit::WebSharedWorker* impl_;
  string16 name_;
  bool started_;
  GURL url_;
  scoped_ptr<SharedWorkerDevToolsAgent> worker_devtools_agent_;

  typedef std::pair<int, int> PendingConnectInfo;
  typedef std::vector<PendingConnectInfo> PendingConnectInfoList;
  PendingConnectInfoList pending_connects_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerStub);
};

#endif  // CONTENT_WORKER_WEBSHAREDWORKER_STUB_H_
