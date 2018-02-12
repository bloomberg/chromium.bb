// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/renderer/worker_thread.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/WebKit/common/service_worker/service_worker_object.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_registration.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_state.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
class Message;
}

namespace content {

class ThreadSafeSender;
class WebServiceWorkerImpl;

// This class manages communication with the browser process about
// registration of the service worker, exposed to renderer and worker
// scripts through methods like navigator.registerServiceWorker().
class CONTENT_EXPORT ServiceWorkerDispatcher : public WorkerThread::Observer {
 public:
  explicit ServiceWorkerDispatcher(
      scoped_refptr<ThreadSafeSender> thread_safe_sender);
  ~ServiceWorkerDispatcher() override;

  void OnMessageReceived(const IPC::Message& msg);

  // Returns the existing service worker or a newly created one with the given
  // object info.
  scoped_refptr<WebServiceWorkerImpl> GetOrCreateServiceWorker(
      blink::mojom::ServiceWorkerObjectInfoPtr info);

  // Sets the IO thread task runner. This is only called for a
  // ServiceWorkerDispatcher instance on a service worker thread when the thread
  // has just started, and the provided IO thread task runner will be used only
  // for creating WebServiceWorkerImpl later.
  // TODO(leonhsl): Remove this function once we addressed the TODO in
  // WebServiceWorkerImpl about the legacy IPC channel-associated interface.
  void SetIOThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner);

  static ServiceWorkerDispatcher* GetOrCreateThreadSpecificInstance(
      scoped_refptr<ThreadSafeSender> thread_safe_sender);

  // Unlike GetOrCreateThreadSpecificInstance() this doesn't create a new
  // instance if thread-local instance doesn't exist.
  static ServiceWorkerDispatcher* GetThreadSpecificInstance();

 private:
  using WorkerObjectMap = std::map<int, WebServiceWorkerImpl*>;

  friend class ServiceWorkerContextClientTest;
  friend class ServiceWorkerDispatcherTest;
  friend class WebServiceWorkerImpl;

  void AllowReinstantiationForTesting();

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  void OnServiceWorkerStateChanged(int thread_id,
                                   int handle_id,
                                   blink::mojom::ServiceWorkerState state);

  // Keeps map from handle_id to ServiceWorker object.
  void AddServiceWorker(int handle_id, WebServiceWorkerImpl* worker);
  void RemoveServiceWorker(int handle_id);

  // True if another dispatcher is allowed to be created on the same thread
  // after this instance is destructed.
  bool allow_reinstantiation_ = false;

  WorkerObjectMap service_workers_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
