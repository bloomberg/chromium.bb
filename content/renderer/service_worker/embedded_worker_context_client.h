// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_CONTEXT_CLIENT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_CONTEXT_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextClient.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace blink {
class WebDataSource;
class WebServiceWorkerProvider;
}

namespace content {

class ServiceWorkerProviderContext;
class ServiceWorkerScriptContext;
class ThreadSafeSender;

// This class provides access to/from an embedded worker's WorkerGlobalScope.
// Unless otherwise noted, all methods are called on the worker thread.
//
// TODO(kinuko): Currently EW/SW separation is made a little hazily.
// This should implement WebEmbeddedWorkerContextClient
// or sort of it (which doesn't exist yet) rather than
// WebServiceWorkerContextClient if we want to separate them more cleanly,
// or ServiceWorkerScriptContext should be merged into this class
// if we consider EW == SW script context.
class EmbeddedWorkerContextClient
    : public blink::WebServiceWorkerContextClient {
 public:
  // Returns a thread-specific client instance.  This does NOT create a
  // new instance.
  static EmbeddedWorkerContextClient* ThreadSpecificInstance();

  // Called on the main thread.
  EmbeddedWorkerContextClient(int embedded_worker_id,
                              int64 service_worker_version_id,
                              const GURL& service_worker_scope,
                              const GURL& script_url,
                              int worker_devtools_agent_route_id);

  virtual ~EmbeddedWorkerContextClient();

  bool OnMessageReceived(const IPC::Message& msg);

  void Send(IPC::Message* message);

  // WebServiceWorkerContextClient overrides, some of them are just dispatched
  // on to script_context_.
  virtual blink::WebURL scope() const;
  virtual blink::WebServiceWorkerCacheStorage* cacheStorage();
  virtual void didPauseAfterDownload();
  virtual void getClients(const blink::WebServiceWorkerClientQueryOptions&,
                          blink::WebServiceWorkerClientsCallbacks*);
  virtual void openWindow(const blink::WebURL&,
                          blink::WebServiceWorkerClientCallbacks*);
  virtual void setCachedMetadata(const blink::WebURL&,
                                 const char* data,
                                 size_t size);
  virtual void clearCachedMetadata(const blink::WebURL&);
  virtual void workerReadyForInspection();

  // Called on the main thread.
  virtual void workerContextFailedToStart();

  virtual void workerContextStarted(blink::WebServiceWorkerContextProxy* proxy);
  virtual void didEvaluateWorkerScript(bool success);
  virtual void willDestroyWorkerContext();
  virtual void workerContextDestroyed();
  virtual void reportException(const blink::WebString& error_message,
                               int line_number,
                               int column_number,
                               const blink::WebString& source_url);
  virtual void reportConsoleMessage(int source,
                                    int level,
                                    const blink::WebString& message,
                                    int line_number,
                                    const blink::WebString& source_url);
  virtual void sendDevToolsMessage(int call_id,
                                   const blink::WebString& message,
                                   const blink::WebString& state);
  virtual void didHandleActivateEvent(int request_id,
                                      blink::WebServiceWorkerEventResult);
  virtual void didHandleInstallEvent(int request_id,
                                     blink::WebServiceWorkerEventResult result);
  virtual void didHandleFetchEvent(int request_id);
  virtual void didHandleFetchEvent(
      int request_id,
      const blink::WebServiceWorkerResponse& response);
  virtual void didHandleNotificationClickEvent(
      int request_id,
      blink::WebServiceWorkerEventResult result);
  virtual void didHandlePushEvent(int request_id,
                                  blink::WebServiceWorkerEventResult result);
  virtual void didHandleSyncEvent(int request_id);
  virtual void didHandleCrossOriginConnectEvent(int request_id,
                                                bool accept_connection);

  // Called on the main thread.
  virtual blink::WebServiceWorkerNetworkProvider*
      createServiceWorkerNetworkProvider(blink::WebDataSource* data_source);
  virtual blink::WebServiceWorkerProvider* createServiceWorkerProvider();

  virtual void postMessageToClient(
      const blink::WebString& uuid,
      const blink::WebString& message,
      blink::WebMessagePortChannelArray* channels);
  virtual void postMessageToCrossOriginClient(
      const blink::WebCrossOriginServiceWorkerClient& client,
      const blink::WebString& message,
      blink::WebMessagePortChannelArray* channels);
  virtual void focus(const blink::WebString& uuid,
                     blink::WebServiceWorkerClientCallbacks*);
  virtual void skipWaiting(
      blink::WebServiceWorkerSkipWaitingCallbacks* callbacks);
  virtual void claim(blink::WebServiceWorkerClientsClaimCallbacks* callbacks);

  // TODO: Implement DevTools related method overrides.

  int embedded_worker_id() const { return embedded_worker_id_; }
  base::SingleThreadTaskRunner* main_thread_task_runner() const {
    return main_thread_task_runner_.get();
  }
  ThreadSafeSender* thread_safe_sender() { return sender_.get(); }

 private:
  void OnMessageToWorker(int thread_id,
                         int embedded_worker_id,
                         const IPC::Message& message);
  void SendWorkerStarted();
  void SetRegistrationInServiceWorkerGlobalScope();

  const int embedded_worker_id_;
  const int64 service_worker_version_id_;
  const GURL service_worker_scope_;
  const GURL script_url_;
  const int worker_devtools_agent_route_id_;
  scoped_refptr<ThreadSafeSender> sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::TaskRunner> worker_task_runner_;

  scoped_ptr<ServiceWorkerScriptContext> script_context_;
  scoped_refptr<ServiceWorkerProviderContext> provider_context_;

  base::WeakPtrFactory<EmbeddedWorkerContextClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerContextClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_CONTEXT_CLIENT_H_
