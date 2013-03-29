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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"

namespace chrome {

PepperExtensionsCommonHost::PepperExtensionsCommonHost(
    content::RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    extensions::Dispatcher* dispatcher)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      dispatcher_(dispatcher) {
}

PepperExtensionsCommonHost::~PepperExtensionsCommonHost() {
  dispatcher_->request_sender()->InvalidateSource(this);
}

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

  return new PepperExtensionsCommonHost(host, instance, resource, dispatcher);
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

extensions::ChromeV8Context* PepperExtensionsCommonHost::GetContext() {
  WebKit::WebPluginContainer* container =
      renderer_ppapi_host_->GetContainerForInstance(pp_instance());
  if (!container)
    return NULL;

  WebKit::WebFrame* frame = container->element().document().frame();
  v8::HandleScope scope;
  return dispatcher_->v8_context_set().GetByV8Context(
      frame->mainWorldScriptContext());
}

void PepperExtensionsCommonHost::OnResponseReceived(
    const std::string& /* name */,
    int request_id,
    bool success,
    const base::ListValue& response,
    const std::string& /* error */) {
  PendingRequestMap::iterator iter = pending_request_map_.find(request_id);

  // Ignore responses resulted from calls to OnPost().
  if (iter == pending_request_map_.end()) {
    DCHECK_EQ(0u, response.GetSize());
    return;
  }

  linked_ptr<ppapi::host::ReplyMessageContext> context = iter->second;
  pending_request_map_.erase(iter);

  context->params.set_result(success ? PP_OK : PP_ERROR_FAILED);
  SendReply(*context, PpapiPluginMsg_ExtensionsCommon_CallReply(response));
}

int32_t PepperExtensionsCommonHost::OnPost(
    ppapi::host::HostMessageContext* context,
    const std::string& request_name,
    base::ListValue& args) {
  // TODO(yzshen): Add support for calling into JS for APIs that have custom
  // bindings.
  int request_id = dispatcher_->request_sender()->GetNextRequestId();
  dispatcher_->request_sender()->StartRequest(this, request_name, request_id,
                                              false, false, &args);
  return PP_OK;
}

int32_t PepperExtensionsCommonHost::OnCall(
    ppapi::host::HostMessageContext* context,
    const std::string& request_name,
    base::ListValue& args) {
  // TODO(yzshen): Add support for calling into JS for APIs that have custom
  // bindings.
  int request_id = dispatcher_->request_sender()->GetNextRequestId();
  pending_request_map_[request_id] =
      linked_ptr<ppapi::host::ReplyMessageContext>(
          new ppapi::host::ReplyMessageContext(
              context->MakeReplyMessageContext()));

  dispatcher_->request_sender()->StartRequest(this, request_name, request_id,
                                              true, false, &args);
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace chrome

