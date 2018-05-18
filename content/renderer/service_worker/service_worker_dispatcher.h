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
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/renderer/worker_thread.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/serviceworker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/serviceworker/web_service_worker_provider.h"
#include "third_party/blink/public/platform/modules/serviceworker/web_service_worker_registration.h"

namespace content {

class WebServiceWorkerImpl;

// TODO(leonhsl): Later once we changed to manage WebServiceWorkerImpl instances
// per provider, this class will become totally useless and we can eliminate it
// finally.
class CONTENT_EXPORT ServiceWorkerDispatcher : public WorkerThread::Observer {
 public:
  explicit ServiceWorkerDispatcher();
  ~ServiceWorkerDispatcher() override;

  // Returns the existing service worker or a newly created one with the given
  // object info. This is only to be used in service worker execution contexts,
  // please use ServiceWorkerProviderContext::GetOrCreateServiceWorkerObject()
  // instead in service worker client contexts.
  scoped_refptr<WebServiceWorkerImpl> GetOrCreateServiceWorker(
      blink::mojom::ServiceWorkerObjectInfoPtr info);

  static ServiceWorkerDispatcher* GetOrCreateThreadSpecificInstance();

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

  // Keeps map from handle_id to ServiceWorker object. These functions are only
  // to be called by those WebServiceWorkerImpls in service worker execution
  // contexts, i.e. those created by GetOrCreateServiceWorker().
  void AddServiceWorker(int handle_id, WebServiceWorkerImpl* worker);
  void RemoveServiceWorker(int handle_id);

  // True if another dispatcher is allowed to be created on the same thread
  // after this instance is destructed.
  bool allow_reinstantiation_ = false;

  WorkerObjectMap service_workers_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
