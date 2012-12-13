// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_flash_renderer_host.h"

#include "content/common/view_messages.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/render_thread_impl.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {

PepperFlashRendererHost::PepperFlashRendererHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource) {
}

PepperFlashRendererHost::~PepperFlashRendererHost() {
}

int32_t PepperFlashRendererHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFlashRendererHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Flash_GetProxyForURL,
                                      OnMsgGetProxyForURL);
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashRendererHost::OnMsgGetProxyForURL(
    ppapi::host::HostMessageContext* host_context,
    const std::string& url) {
  GURL gurl(url);
  if (!gurl.is_valid())
    return PP_ERROR_FAILED;
  bool result;
  std::string proxy;
  RenderThreadImpl::current()->Send(
      new ViewHostMsg_ResolveProxy(gurl, &result, &proxy));
  if (!result)
    return PP_ERROR_FAILED;
  host_context->reply_msg = PpapiPluginMsg_Flash_GetProxyForURLReply(proxy);
  return PP_OK;
}

}  // namespace content
