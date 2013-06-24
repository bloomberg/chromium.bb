// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_network_proxy_host.h"

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

namespace {

scoped_refptr<net::URLRequestContextGetter>
GetURLRequestContextGetterOnUIThread(int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<net::URLRequestContextGetter> context_getter;
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id);
  if (render_process_host && render_process_host->GetBrowserContext()) {
    context_getter = render_process_host->GetBrowserContext()->
        GetRequestContextForRenderProcess(render_process_id);
  }
  return context_getter;
}

}  // namespace

PepperNetworkProxyHost::PepperNetworkProxyHost(BrowserPpapiHost* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      proxy_service_(NULL),
      waiting_for_proxy_service_(true),
      weak_factory_(this) {
  int render_process_id(0), render_view_id_unused(0);
  host->GetRenderViewIDsForInstance(instance,
                                    &render_process_id,
                                    &render_view_id_unused);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetURLRequestContextGetterOnUIThread, render_process_id),
      base::Bind(&PepperNetworkProxyHost::DidGetURLRequestContextGetter,
                 weak_factory_.GetWeakPtr()));
}

PepperNetworkProxyHost::~PepperNetworkProxyHost() {
  while (!pending_requests_.empty()) {
    // If the proxy_service_ is NULL, we shouldn't have any outstanding
    // requests.
    DCHECK(proxy_service_);
    net::ProxyService::PacRequest* request = pending_requests_.front();
    proxy_service_->CancelPacRequest(request);
    pending_requests_.pop();
  }
}

void PepperNetworkProxyHost::DidGetURLRequestContextGetter(
    scoped_refptr<net::URLRequestContextGetter> context_getter) {
  if (context_getter->GetURLRequestContext())
    proxy_service_ = context_getter->GetURLRequestContext()->proxy_service();
  waiting_for_proxy_service_ = false;
  if (!proxy_service_) {
    DLOG_IF(WARNING, proxy_service_)
        << "Failed to find a ProxyService for Pepper plugin.";
  }
  TryToSendUnsentRequests();
}

int32_t PepperNetworkProxyHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperNetworkProxyHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_NetworkProxy_GetProxyForURL, OnMsgGetProxyForURL)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperNetworkProxyHost::OnMsgGetProxyForURL(
    ppapi::host::HostMessageContext* context,
    const std::string& url) {
  GURL gurl(url);
  if (gurl.is_valid()) {
    UnsentRequest request = { gurl, context->MakeReplyMessageContext() };
    unsent_requests_.push(request);
    TryToSendUnsentRequests();
  } else {
    SendFailureReply(PP_ERROR_BADARGUMENT, context->MakeReplyMessageContext());
  }
  return PP_OK_COMPLETIONPENDING;
}

void PepperNetworkProxyHost::TryToSendUnsentRequests() {
  if (waiting_for_proxy_service_)
    return;

  while (!unsent_requests_.empty()) {
    const UnsentRequest& request = unsent_requests_.front();
    if (!proxy_service_) {
      SendFailureReply(PP_ERROR_FAILED, request.reply_context);
    } else {
      // Everything looks valid, so try to resolve the proxy.
      net::ProxyInfo* proxy_info = new net::ProxyInfo;
      net::ProxyService::PacRequest* pending_request = NULL;
      base::Callback<void (int)> callback =
          base::Bind(&PepperNetworkProxyHost::OnResolveProxyCompleted,
                     weak_factory_.GetWeakPtr(),
                     request.reply_context,
                     base::Owned(proxy_info));
      int result = proxy_service_->ResolveProxy(request.url,
                                                proxy_info,
                                                callback,
                                                &pending_request,
                                                net::BoundNetLog());
      pending_requests_.push(pending_request);
      // If it was handled synchronously, we must run the callback now;
      // proxy_service_ won't run it for us in this case.
      if (result != net::ERR_IO_PENDING)
        callback.Run(result);
    }
    unsent_requests_.pop();
  }
}

void PepperNetworkProxyHost::OnResolveProxyCompleted(
    ppapi::host::ReplyMessageContext context,
    net::ProxyInfo* proxy_info,
    int result) {
  pending_requests_.pop();

  if (result != net::OK) {
    // TODO(dmichael): Add appropriate error codes to yzshen's conversion
    //                 function, and call that function here.
    context.params.set_result(PP_ERROR_FAILED);
  }
  host()->SendReply(context,
                    PpapiPluginMsg_NetworkProxy_GetProxyForURLReply(
                        proxy_info->ToPacString()));
}

void PepperNetworkProxyHost::SendFailureReply(
    int32_t error,
    ppapi::host::ReplyMessageContext context) {
  context.params.set_result(error);
  host()->SendReply(context,
                    PpapiPluginMsg_NetworkProxy_GetProxyForURLReply(
                        std::string()));
}

}  // namespace content
