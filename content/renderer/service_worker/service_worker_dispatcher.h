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
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_object.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_state.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
class Message;
}

namespace content {

class ServiceWorkerHandleReference;
class ThreadSafeSender;
class WebServiceWorkerImpl;

// This class manages communication with the browser process about
// registration of the service worker, exposed to renderer and worker
// scripts through methods like navigator.registerServiceWorker().
class CONTENT_EXPORT ServiceWorkerDispatcher : public WorkerThread::Observer {
 public:
  ServiceWorkerDispatcher(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner);
  ~ServiceWorkerDispatcher() override;

  void OnMessageReceived(const IPC::Message& msg);

  // Returns the existing service worker or a newly created one with the given
  // handle reference. Returns nullptr if the given reference is invalid.
  scoped_refptr<WebServiceWorkerImpl> GetOrCreateServiceWorker(
      std::unique_ptr<ServiceWorkerHandleReference> handle_ref);

  static ServiceWorkerDispatcher* GetOrCreateThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner);

  // Unlike GetOrCreateThreadSpecificInstance() this doesn't create a new
  // instance if thread-local instance doesn't exist.
  static ServiceWorkerDispatcher* GetThreadSpecificInstance();

  base::SingleThreadTaskRunner* main_thread_task_runner() {
    return main_thread_task_runner_.get();
  }

  scoped_refptr<ThreadSafeSender> thread_safe_sender() {
    return thread_safe_sender_;
  }

 private:
  using WorkerObjectMap = std::map<int, WebServiceWorkerImpl*>;

  friend class ServiceWorkerDispatcherTest;
  friend class WebServiceWorkerImpl;

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  void OnServiceWorkerStateChanged(int thread_id,
                                   int handle_id,
                                   blink::mojom::ServiceWorkerState state);

  // Keeps map from handle_id to ServiceWorker object.
  void AddServiceWorker(int handle_id, WebServiceWorkerImpl* worker);
  void RemoveServiceWorker(int handle_id);

  WorkerObjectMap service_workers_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
