// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_host_resolver_private_message_filter.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_lookup_request.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/socket_permission_request.h"
#include "net/base/address_list.h"
#include "net/dns/host_resolver.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"

using ppapi::host::ReplyMessageContext;

namespace content {

namespace {

void PrepareRequestInfo(const PP_HostResolver_Private_Hint& hint,
                        net::HostResolver::RequestInfo* request_info) {
  DCHECK(request_info);

  net::AddressFamily address_family;
  switch (hint.family) {
    case PP_NETADDRESSFAMILY_IPV4:
      address_family = net::ADDRESS_FAMILY_IPV4;
      break;
    case PP_NETADDRESSFAMILY_IPV6:
      address_family = net::ADDRESS_FAMILY_IPV6;
      break;
    default:
      address_family = net::ADDRESS_FAMILY_UNSPECIFIED;
  }
  request_info->set_address_family(address_family);

  net::HostResolverFlags host_resolver_flags = 0;
  if (hint.flags & PP_HOST_RESOLVER_FLAGS_CANONNAME)
    host_resolver_flags |= net::HOST_RESOLVER_CANONNAME;
  if (hint.flags & PP_HOST_RESOLVER_FLAGS_LOOPBACK_ONLY)
    host_resolver_flags |= net::HOST_RESOLVER_LOOPBACK_ONLY;
  request_info->set_host_resolver_flags(host_resolver_flags);
}

void CreateNetAddressListFromAddressList(
    const net::AddressList& list,
    std::vector<PP_NetAddress_Private>* net_address_list) {
  DCHECK(net_address_list);

  net_address_list->clear();
  net_address_list->reserve(list.size());

  PP_NetAddress_Private address;
  for (size_t i = 0; i < list.size(); ++i) {
    if (!ppapi::NetAddressPrivateImpl::IPEndPointToNetAddress(list[i].address(),
                                                              list[i].port(),
                                                              &address)) {
      net_address_list->clear();
      return;
    }
    net_address_list->push_back(address);
  }
}

}  // namespace

PepperHostResolverPrivateMessageFilter::PepperHostResolverPrivateMessageFilter(
    BrowserPpapiHostImpl* host, PP_Instance instance)
    : external_plugin_(host->external_plugin()),
      render_process_id_(0),
      render_view_id_(0) {
  DCHECK(host);

  if (!host->GetRenderViewIDsForInstance(
          instance,
          &render_process_id_,
          &render_view_id_)) {
    NOTREACHED();
  }
}

PepperHostResolverPrivateMessageFilter::
~PepperHostResolverPrivateMessageFilter() {
}

scoped_refptr<base::TaskRunner>
PepperHostResolverPrivateMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  if (message.type() == PpapiHostMsg_HostResolverPrivate_Resolve::ID)
    return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  return NULL;
}

int32_t PepperHostResolverPrivateMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperHostResolverPrivateMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_HostResolverPrivate_Resolve, OnMsgResolve)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperHostResolverPrivateMessageFilter::OnMsgResolve(
    const ppapi::host::HostMessageContext* context,
    const ppapi::HostPortPair& host_port,
    const PP_HostResolver_Private_Hint& hint) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check plugin permissions.
  SocketPermissionRequest request(
      content::SocketPermissionRequest::NONE, "", 0);
  RenderViewHost* render_view_host = RenderViewHost::FromID(render_process_id_,
                                                            render_view_id_);
  if (!render_view_host ||
      !pepper_socket_utils::CanUseSocketAPIs(external_plugin_,
                                             request,
                                             render_view_host)) {
    return PP_ERROR_FAILED;
  }

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id_);
  if (!render_process_host)
    return PP_ERROR_FAILED;
  BrowserContext* browser_context = render_process_host->GetBrowserContext();
  if (!browser_context || !browser_context->GetResourceContext())
    return PP_ERROR_FAILED;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperHostResolverPrivateMessageFilter::DoResolve, this,
                 context->MakeReplyMessageContext(),
                 host_port,
                 hint,
                 browser_context->GetResourceContext()));
  return PP_OK_COMPLETIONPENDING;
}

void PepperHostResolverPrivateMessageFilter::DoResolve(
    const ReplyMessageContext& context,
    const ppapi::HostPortPair& host_port,
    const PP_HostResolver_Private_Hint& hint,
    ResourceContext* resource_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::HostResolver* host_resolver = resource_context->GetHostResolver();
  if (!host_resolver) {
    SendResolveError(context);
    return;
  }

  net::HostResolver::RequestInfo request_info(
      net::HostPortPair(host_port.host, host_port.port));
  PrepareRequestInfo(hint, &request_info);

  scoped_ptr<ReplyMessageContext> bound_info(new ReplyMessageContext(context));

  // The lookup request will delete itself on completion.
  PepperLookupRequest<ReplyMessageContext>* lookup_request =
      new PepperLookupRequest<ReplyMessageContext>(
          host_resolver,
          request_info,
          bound_info.release(),
          base::Bind(&PepperHostResolverPrivateMessageFilter::OnLookupFinished,
                     this));
  lookup_request->Start();
}

void PepperHostResolverPrivateMessageFilter::OnLookupFinished(
    int result,
    const net::AddressList& addresses,
    const ReplyMessageContext& context) {
  if (result != net::OK) {
    SendResolveError(context);
  } else {
    const std::string& canonical_name = addresses.canonical_name();
    NetAddressList net_address_list;
    CreateNetAddressListFromAddressList(addresses, &net_address_list);
    if (net_address_list.empty())
      SendResolveError(context);
    else
      SendResolveReply(PP_OK, canonical_name, net_address_list, context);
  }
}

void PepperHostResolverPrivateMessageFilter::SendResolveReply(
    int result,
    const std::string& canonical_name,
    const NetAddressList& net_address_list,
    const ReplyMessageContext& context) {
  ReplyMessageContext reply_context = context;
  reply_context.params.set_result(result);
  SendReply(reply_context,
            PpapiPluginMsg_HostResolverPrivate_ResolveReply(
                canonical_name, net_address_list));
}

void PepperHostResolverPrivateMessageFilter::SendResolveError(
    const ReplyMessageContext& context) {
  SendResolveReply(PP_ERROR_FAILED, std::string(), NetAddressList(), context);
}

}  // namespace content
