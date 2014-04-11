// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_

#include <map>

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerProvider.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerState.h"

class GURL;

namespace blink {
class WebURL;
}

namespace IPC {
class Message;
}

namespace content {

class ServiceWorkerMessageFilter;
struct ServiceWorkerObjectInfo;
class ThreadSafeSender;
class WebServiceWorkerImpl;

// This class manages communication with the browser process about
// registration of the service worker, exposed to renderer and worker
// scripts through methods like navigator.registerServiceWorker().
class ServiceWorkerDispatcher : public WorkerTaskRunner::Observer {
 public:
  explicit ServiceWorkerDispatcher(ThreadSafeSender* thread_safe_sender);
  virtual ~ServiceWorkerDispatcher();

  void OnMessageReceived(const IPC::Message& msg);
  bool Send(IPC::Message* msg);

  // Corresponds to navigator.serviceWorker.register()
  void RegisterServiceWorker(
      int provider_id,
      const GURL& pattern,
      const GURL& script_url,
      blink::WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks);
  // Corresponds to navigator.serviceWorker.unregister()
  void UnregisterServiceWorker(
      int provider_id,
      const GURL& pattern,
      blink::WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks);

  // Called when navigator.serviceWorker is instantiated or detached
  // for a document whose provider can be identified by |provider_id|.
  void AddScriptClient(int provider_id,
                       blink::WebServiceWorkerProviderClient* client);
  void RemoveScriptClient(int provider_id);

  // |thread_safe_sender| needs to be passed in because if the call leads to
  // construction it will be needed.
  static ServiceWorkerDispatcher* GetOrCreateThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender);

  // Unlike GetOrCreateThreadSpecificInstance() this doesn't create a new
  // instance if thread-local instance doesn't exist.
  static ServiceWorkerDispatcher* GetThreadSpecificInstance();

 private:
  typedef IDMap<blink::WebServiceWorkerProvider::WebServiceWorkerCallbacks,
      IDMapOwnPointer> CallbackMap;
  typedef std::map<int, blink::WebServiceWorkerProviderClient*> ScriptClientMap;
  typedef std::map<int, WebServiceWorkerImpl*> ServiceWorkerMap;

  friend class WebServiceWorkerImpl;

  // WorkerTaskRunner::Observer implementation.
  virtual void OnWorkerRunLoopStopped() OVERRIDE;

  void OnRegistered(int thread_id,
                    int request_id,
                    const ServiceWorkerObjectInfo& info);
  void OnUnregistered(int thread_id,
                      int request_id);
  void OnRegistrationError(int thread_id,
                           int request_id,
                           blink::WebServiceWorkerError::ErrorType error_type,
                           const base::string16& message);
  void OnServiceWorkerStateChanged(int handle_id,
                                   blink::WebServiceWorkerState state);

  // Keeps map from handle_id to ServiceWorker object.
  void AddServiceWorker(int handle_id, WebServiceWorkerImpl* worker);
  void RemoveServiceWorker(int handle_id);

  CallbackMap pending_callbacks_;
  ScriptClientMap script_clients_;
  ServiceWorkerMap service_workers_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
