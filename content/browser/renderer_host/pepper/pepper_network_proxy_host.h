// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_NETWORK_PROXY_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_NETWORK_PROXY_HOST_H_

#include <queue>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "net/proxy/proxy_service.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace net {
class ProxyInfo;
class URLRequestContextGetter;
}

namespace ppapi {
namespace host {
struct ReplyMessageContext;
}
}

namespace content {

class BrowserPpapiHost;

// The host for PPB_NetworkProxy. This class runs exclusively on the IO thread.
class CONTENT_EXPORT PepperNetworkProxyHost : public ppapi::host::ResourceHost {
 public:
  PepperNetworkProxyHost(BrowserPpapiHost* host,
                         PP_Instance instance,
                         PP_Resource resource);

  virtual ~PepperNetworkProxyHost();

 private:
  // We retrieve the appropriate URLRequestContextGetter on the UI thread
  // and pass it to this function, which uses it to retrieve proxy_service_.
  void DidGetURLRequestContextGetter(
      scoped_refptr<net::URLRequestContextGetter> context_getter);

  // ResourceHost implementation.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  int32_t OnMsgGetProxyForURL(ppapi::host::HostMessageContext* context,
                              const std::string& url);

  // If we have a valid proxy_service_, send all messages in unsent_requests_.
  void TryToSendUnsentRequests();

  void OnResolveProxyCompleted(ppapi::host::ReplyMessageContext context,
                               net::ProxyInfo* proxy_info,
                               int result);
  void SendFailureReply(int32_t error,
                        ppapi::host::ReplyMessageContext context);

  net::ProxyService* proxy_service_;
  bool waiting_for_proxy_service_;

  // We have to get the URLRequestContextGetter from the UI thread before we
  // can retrieve proxy_service_. If we receive any calls for GetProxyForURL
  // before proxy_service_ is available, we save them in unsent_requests_.
  struct UnsentRequest {
    GURL url;
    ppapi::host::ReplyMessageContext reply_context;
  };
  std::queue<UnsentRequest> unsent_requests_;

  // Requests awaiting a response from ProxyService. We need to store these so
  // that we can cancel them if we get destroyed.
  std::queue<net::ProxyService::PacRequest*> pending_requests_;

  base::WeakPtrFactory<PepperNetworkProxyHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperNetworkProxyHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_NETWORK_PROXY_HOST_H_
