// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/pepper_extensions_common_host.h"

#include "base/values.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace chrome {

PepperExtensionsCommonHost::PepperExtensionsCommonHost(
    content::RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host) {
}

PepperExtensionsCommonHost::~PepperExtensionsCommonHost() {
}

// static
PepperExtensionsCommonHost* PepperExtensionsCommonHost::Create(
    content::RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource) {
  return new PepperExtensionsCommonHost(host, instance, resource);
}

int32_t PepperExtensionsCommonHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperExtensionsCommonHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_ExtensionsCommon_Post,
                                      OnPost)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_ExtensionsCommon_Call,
                                      OnCall)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperExtensionsCommonHost::OnPost(
    ppapi::host::HostMessageContext* context,
    const std::string& request_name,
    const base::ListValue& args) {
  // TODO(yzshen): Implement it.
  return PP_ERROR_NOTSUPPORTED;
}

int32_t PepperExtensionsCommonHost::OnCall(
    ppapi::host::HostMessageContext* context,
    const std::string& request_name,
    const base::ListValue& args) {
  // TODO(yzshen): Implement it.
  return PP_ERROR_NOTSUPPORTED;
}

}  // namespace chrome

