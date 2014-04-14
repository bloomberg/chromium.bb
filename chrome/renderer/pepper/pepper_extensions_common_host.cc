// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/pepper_extensions_common_host.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"

namespace {
void DoNothing(bool success,
               const base::ListValue& response,
               const std::string& error) {}
}

PepperExtensionsCommonHost::PepperExtensionsCommonHost(
    content::RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    extensions::PepperRequestProxy* pepper_request_proxy)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      pepper_request_proxy_(pepper_request_proxy),
      weak_factory_(this) {}

PepperExtensionsCommonHost::~PepperExtensionsCommonHost() {}

// static
PepperExtensionsCommonHost* PepperExtensionsCommonHost::Create(
    content::RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource) {
  content::RenderView* render_view = host->GetRenderViewForInstance(instance);
  if (!render_view)
    return NULL;
  extensions::ExtensionHelper* extension_helper =
      extensions::ExtensionHelper::Get(render_view);
  if (!extension_helper)
    return NULL;
  extensions::Dispatcher* dispatcher = extension_helper->dispatcher();
  if (!dispatcher)
    return NULL;
  blink::WebPluginContainer* container =
      host->GetContainerForInstance(instance);
  if (!container)
    return NULL;
  blink::WebFrame* frame = container->element().document().frame();
  if (!frame)
    return NULL;
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  extensions::ChromeV8Context* context =
      dispatcher->v8_context_set().GetByV8Context(
          frame->mainWorldScriptContext());
  if (!context)
    return NULL;

  return new PepperExtensionsCommonHost(
      host, instance, resource, context->pepper_request_proxy());
}

int32_t PepperExtensionsCommonHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperExtensionsCommonHost, msg)
  PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_ExtensionsCommon_Post, OnPost)
  PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_ExtensionsCommon_Call, OnCall)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperExtensionsCommonHost::OnResponseReceived(
    ppapi::host::ReplyMessageContext reply_context,
    bool success,
    const base::ListValue& response,
    const std::string& /* error */) {
  reply_context.params.set_result(success ? PP_OK : PP_ERROR_FAILED);
  SendReply(reply_context, PpapiPluginMsg_ExtensionsCommon_CallReply(response));
}

int32_t PepperExtensionsCommonHost::OnPost(
    ppapi::host::HostMessageContext* context,
    const std::string& request_name,
    const base::ListValue& args) {
  std::string error;
  bool success = pepper_request_proxy_->StartRequest(
      base::Bind(&DoNothing), request_name, args, &error);
  return success ? PP_OK : PP_ERROR_FAILED;
}

int32_t PepperExtensionsCommonHost::OnCall(
    ppapi::host::HostMessageContext* context,
    const std::string& request_name,
    const base::ListValue& args) {
  std::string error;
  bool success = pepper_request_proxy_->StartRequest(
      base::Bind(&PepperExtensionsCommonHost::OnResponseReceived,
                 weak_factory_.GetWeakPtr(),
                 context->MakeReplyMessageContext()),
      request_name,
      args,
      &error);
  return success ? PP_OK_COMPLETIONPENDING : PP_ERROR_FAILED;
}
