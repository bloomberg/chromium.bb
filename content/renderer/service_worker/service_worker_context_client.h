// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_EMBEDDED_WORKER_CLIENT_H_
#define CONTENT_CHILD_SERVICE_WORKER_EMBEDDED_WORKER_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextClient.h"
#include "url/gurl.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class ThreadSafeSender;

// This class provides access to/from an embedded worker's WorkerGlobalScope.
// All methods other than the constructor (it's created on the main thread)
// are called on the worker thread.
class ServiceWorkerContextClient
    : public blink::WebServiceWorkerContextClient {
 public:
  // Returns a thread-specific client instance.  This does NOT create a
  // new instance.
  static ServiceWorkerContextClient* ThreadSpecificInstance();

  ServiceWorkerContextClient(int embedded_worker_id,
                             int64 service_worker_version_id,
                             const GURL& script_url);

  virtual ~ServiceWorkerContextClient();

  bool OnMessageReceived(const IPC::Message& msg);

  // WebServiceWorkerContextClient overrides.
  virtual void workerContextFailedToStart();
  virtual void workerContextStarted(blink::WebServiceWorkerContextProxy* proxy);
  virtual void workerContextDestroyed();

  // TODO: Implement DevTools related method overrides.

  int embedded_worker_id() const { return embedded_worker_id_; }

 private:
  const int embedded_worker_id_;
  const int64 service_worker_version_id_;
  const GURL script_url_;
  scoped_refptr<ThreadSafeSender> sender_;
  scoped_refptr<base::MessageLoopProxy> main_thread_proxy_;

  blink::WebServiceWorkerContextProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextClient);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_EMBEDDED_WORKER_CLIENT_H_
