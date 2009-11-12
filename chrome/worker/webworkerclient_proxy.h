// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WEBWORKERCLIENT_PROXY_H_
#define CHROME_WORKER_WEBWORKERCLIENT_PROXY_H_

#include <vector>

#include "base/basictypes.h"
#include "base/task.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/WebKit/chromium/public/WebWorkerClient.h"

namespace WebKit {
class WebWorker;
}

class WebWorkerStubBase;

// This class receives IPCs from the renderer and calls the WebCore::Worker
// implementation (after the data types have been converted by glue code).  It
// is also called by the worker code and converts these function calls into
// IPCs that are sent to the renderer, where they're converted back to function
// calls by WebWorkerProxy.
class WebWorkerClientProxy : public WebKit::WebWorkerClient {
 public:
  WebWorkerClientProxy(int route_id, WebWorkerStubBase* stub);
  ~WebWorkerClientProxy();

  // WebWorkerClient implementation.
  virtual void postMessageToWorkerObject(
      const WebKit::WebString& message,
      const WebKit::WebMessagePortChannelArray& channel);
  virtual void postExceptionToWorkerObject(
      const WebKit::WebString& error_message,
      int line_number,
      const WebKit::WebString& source_url);
  virtual void postConsoleMessageToWorkerObject(
      int destination,
      int source,
      int type,
      int level,
      const WebKit::WebString& message,
      int line_number,
      const WebKit::WebString& source_url);
  virtual void confirmMessageFromWorkerObject(bool has_pending_activity);
  virtual void reportPendingActivity(bool has_pending_activity);
  virtual void workerContextClosed();
  virtual void workerContextDestroyed();
  virtual WebKit::WebWorker* createWorker(WebKit::WebWorkerClient* client);

  virtual WebKit::WebNotificationPresenter* notificationPresenter() {
    // TODO(johnnyg): Notifications are not yet hooked up to workers.
    // Coming soon.
    NOTREACHED();
    return NULL;
  }

  void EnsureWorkerContextTerminates();

 private:
  bool Send(IPC::Message* message);

  int route_id_;
  WebWorkerStubBase* stub_;
  ScopedRunnableMethodFactory<WebWorkerClientProxy> kill_process_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebWorkerClientProxy);
};

#endif  // CHROME_WORKER_WEBWORKERCLIENT_PROXY_H_
