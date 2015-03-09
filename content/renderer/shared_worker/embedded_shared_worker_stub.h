// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_
#define CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_

#include <vector>

#include "content/child/child_message_filter.h"
#include "content/child/scoped_child_process_reference.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebContentSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebSharedWorkerClient.h"
#include "url/gurl.h"

namespace blink {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebMessagePortChannel;
class WebNotificationPresenter;
class WebSecurityOrigin;
class WebSharedWorker;
class WebWorkerContentSettingsClientProxy;
}

namespace content {
class SharedWorkerDevToolsAgent;
class WebApplicationCacheHostImpl;
class WebMessagePortChannelImpl;

class EmbeddedSharedWorkerStub : public IPC::Listener,
                                 public blink::WebSharedWorkerClient {
 public:
  EmbeddedSharedWorkerStub(
      const GURL& url,
      const base::string16& name,
      const base::string16& content_security_policy,
      blink::WebContentSecurityPolicyType security_policy_type,
      bool pause_on_start,
      int route_id);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  // blink::WebSharedWorkerClient implementation.
  virtual void workerContextClosed() override;
  virtual void workerContextDestroyed() override;
  virtual void workerReadyForInspection() override;
  virtual void workerScriptLoaded() override;
  virtual void workerScriptLoadFailed() override;
  virtual void selectAppCacheID(long long) override;
  virtual blink::WebNotificationPresenter* notificationPresenter() override;
  virtual blink::WebApplicationCacheHost* createApplicationCacheHost(
      blink::WebApplicationCacheHostClient*) override;
  virtual blink::WebWorkerContentSettingsClientProxy*
      createWorkerContentSettingsClientProxy(
          const blink::WebSecurityOrigin& origin) override;
  virtual blink::WebServiceWorkerNetworkProvider*
      createServiceWorkerNetworkProvider(blink::WebDataSource*) override;
  virtual void sendDevToolsMessage(
      int call_id,
      const blink::WebString& message,
      const blink::WebString& state) override;

 private:
  ~EmbeddedSharedWorkerStub() override;

  void Shutdown();
  bool Send(IPC::Message* message);

  // WebSharedWorker will own |channel|.
  void ConnectToChannel(WebMessagePortChannelImpl* channel);

  void OnConnect(int sent_message_port_id, int routing_id);
  void OnTerminateWorkerContext();

  int route_id_;
  base::string16 name_;
  bool runing_;
  GURL url_;
  blink::WebSharedWorker* impl_;
  scoped_ptr<SharedWorkerDevToolsAgent> worker_devtools_agent_;

  typedef std::vector<WebMessagePortChannelImpl*> PendingChannelList;
  PendingChannelList pending_channels_;

  ScopedChildProcessReference process_ref_;
  WebApplicationCacheHostImpl* app_cache_host_;
  DISALLOW_COPY_AND_ASSIGN(EmbeddedSharedWorkerStub);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_
