// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/js_renderer_messaging_service.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/common/child_process_host.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_bindings_system.h"
#include "extensions/renderer/extension_port.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/messaging_bindings.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebScopedWindowFocusAllowedIndicator.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "v8/include/v8.h"

namespace extensions {

using v8_helpers::ToV8String;

namespace {

void HasMessagePort(const PortId& port_id,
                    bool* has_port,
                    ScriptContext* script_context) {
  if (*has_port)
    return;  // Stop checking if the port was found.

  MessagingBindings* bindings = MessagingBindings::ForContext(script_context);
  DCHECK(bindings);
  // No need for |=; we know this is false right now from above.
  *has_port = bindings->GetPortWithId(port_id) != nullptr;
}

void DispatchOnConnectToScriptContext(
    const PortId& target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo* source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id,
    bool* port_created,
    ScriptContext* script_context) {
  MessagingBindings* bindings = MessagingBindings::ForContext(script_context);
  DCHECK(bindings);

  // If the channel was opened by this same context, ignore it. This should only
  // happen when messages are sent to an entire process (rather than a single
  // frame) as an optimization; otherwise the browser process filters this out.
  if (bindings->context_id() == target_port_id.context_id)
    return;

  // First, determine the event we'll use to connect.
  std::string target_extension_id = script_context->GetExtensionID();
  bool is_external = info.source_id != target_extension_id;
  std::string event_name;
  if (channel_name == "chrome.extension.sendRequest") {
    event_name =
        is_external ? "extension.onRequestExternal" : "extension.onRequest";
  } else if (channel_name == "chrome.runtime.sendMessage") {
    event_name =
        is_external ? "runtime.onMessageExternal" : "runtime.onMessage";
  } else {
    event_name =
        is_external ? "runtime.onConnectExternal" : "runtime.onConnect";
  }

  ExtensionBindingsSystem* bindings_system =
      ExtensionsRendererClient::Get()->GetDispatcher()->bindings_system();
  // If there are no listeners for the given event, then we know the port won't
  // be used in this context.
  if (!bindings_system->HasEventListenerInContext(event_name, script_context)) {
    return;
  }
  *port_created = true;

  ExtensionPort* port = bindings->CreateNewPortWithId(target_port_id);

  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  const std::string& source_url_spec = info.source_url.spec();
  const Extension* extension = script_context->extension();

  v8::Local<v8::Value> tab = v8::Null(isolate);
  v8::Local<v8::Value> tls_channel_id_value = v8::Undefined(isolate);
  v8::Local<v8::Value> guest_process_id = v8::Undefined(isolate);
  v8::Local<v8::Value> guest_render_frame_routing_id = v8::Undefined(isolate);

  if (extension) {
    if (!source->tab.empty() && !extension->is_platform_app()) {
      tab = content::V8ValueConverter::Create()->ToV8Value(
          &source->tab, script_context->v8_context());
    }

    ExternallyConnectableInfo* externally_connectable =
        ExternallyConnectableInfo::Get(extension);
    if (externally_connectable &&
        externally_connectable->accepts_tls_channel_id) {
      v8::Local<v8::String> v8_tls_channel_id;
      if (ToV8String(isolate, tls_channel_id.c_str(), &v8_tls_channel_id))
        tls_channel_id_value = v8_tls_channel_id;
    }

    if (info.guest_process_id != content::ChildProcessHost::kInvalidUniqueID) {
      guest_process_id = v8::Integer::New(isolate, info.guest_process_id);
      guest_render_frame_routing_id =
          v8::Integer::New(isolate, info.guest_render_frame_routing_id);
    }
  }

  v8::Local<v8::String> v8_channel_name;
  v8::Local<v8::String> v8_source_id;
  v8::Local<v8::String> v8_target_extension_id;
  v8::Local<v8::String> v8_source_url_spec;
  if (!ToV8String(isolate, channel_name.c_str(), &v8_channel_name) ||
      !ToV8String(isolate, info.source_id.c_str(), &v8_source_id) ||
      !ToV8String(isolate, target_extension_id.c_str(),
                  &v8_target_extension_id) ||
      !ToV8String(isolate, source_url_spec.c_str(), &v8_source_url_spec)) {
    NOTREACHED() << "dispatchOnConnect() passed non-string argument";
    return;
  }

  v8::Local<v8::Value> arguments[] = {
      // portId
      v8::Integer::New(isolate, port->js_id()),
      // channelName
      v8_channel_name,
      // sourceTab
      tab,
      // source_frame_id
      v8::Integer::New(isolate, source->frame_id),
      // guestProcessId
      guest_process_id,
      // guestRenderFrameRoutingId
      guest_render_frame_routing_id,
      // sourceExtensionId
      v8_source_id,
      // targetExtensionId
      v8_target_extension_id,
      // sourceUrl
      v8_source_url_spec,
      // tlsChannelId
      tls_channel_id_value,
  };

  // Note: this can execute asynchronously if JS is suspended.
  script_context->module_system()->CallModuleMethodSafe(
      "messaging", "dispatchOnConnect", arraysize(arguments), arguments);
}

void DeliverMessageToScriptContext(const Message& message,
                                   const PortId& target_port_id,
                                   ScriptContext* script_context) {
  MessagingBindings* bindings = MessagingBindings::ForContext(script_context);
  DCHECK(bindings);
  ExtensionPort* port = bindings->GetPortWithId(target_port_id);
  if (!port)
    return;

  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> port_id_handle =
      v8::Integer::New(isolate, port->js_id());

  v8::Local<v8::String> v8_data;
  if (!ToV8String(isolate, message.data.c_str(), &v8_data))
    return;
  std::vector<v8::Local<v8::Value>> arguments;
  arguments.push_back(v8_data);
  arguments.push_back(port_id_handle);

  std::unique_ptr<blink::WebScopedUserGesture> web_user_gesture;
  std::unique_ptr<blink::WebScopedWindowFocusAllowedIndicator>
      allow_window_focus;
  if (message.user_gesture) {
    web_user_gesture.reset(
        new blink::WebScopedUserGesture(script_context->web_frame()));

    if (script_context->web_frame()) {
      blink::WebDocument document = script_context->web_frame()->GetDocument();
      allow_window_focus.reset(
          new blink::WebScopedWindowFocusAllowedIndicator(&document));
    }
  }

  script_context->module_system()->CallModuleMethodSafe(
      "messaging", "dispatchOnMessage", &arguments);
}

void DispatchOnDisconnectToScriptContext(const PortId& port_id,
                                         const std::string& error_message,
                                         ScriptContext* script_context) {
  MessagingBindings* bindings = MessagingBindings::ForContext(script_context);
  DCHECK(bindings);
  ExtensionPort* port = bindings->GetPortWithId(port_id);
  if (!port)
    return;

  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  std::vector<v8::Local<v8::Value>> arguments;
  arguments.push_back(v8::Integer::New(isolate, port->js_id()));
  v8::Local<v8::String> v8_error_message;
  if (!error_message.empty())
    ToV8String(isolate, error_message.c_str(), &v8_error_message);
  if (!v8_error_message.IsEmpty()) {
    arguments.push_back(v8_error_message);
  } else {
    arguments.push_back(v8::Null(isolate));
  }

  script_context->module_system()->CallModuleMethodSafe(
      "messaging", "dispatchOnDisconnect", &arguments);
}

}  // namespace

JSRendererMessagingService::JSRendererMessagingService() {}
JSRendererMessagingService::~JSRendererMessagingService() {}

void JSRendererMessagingService::ValidateMessagePort(
    const ScriptContextSet& context_set,
    const PortId& port_id,
    content::RenderFrame* render_frame) {
  int routing_id = render_frame->GetRoutingID();

  bool has_port = false;
  context_set.ForEach(render_frame,
                      base::Bind(&HasMessagePort, port_id, &has_port));

  // A reply is only sent if the port is missing, because the port is assumed to
  // exist unless stated otherwise.
  if (!has_port) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_CloseMessagePort(routing_id, port_id, false));
  }
}

void JSRendererMessagingService::DispatchOnConnect(
    const ScriptContextSet& context_set,
    const PortId& target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo& source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id,
    content::RenderFrame* restrict_to_render_frame) {
  DCHECK(!target_port_id.is_opener);
  int routing_id = restrict_to_render_frame
                       ? restrict_to_render_frame->GetRoutingID()
                       : MSG_ROUTING_NONE;
  bool port_created = false;
  context_set.ForEach(
      info.target_id, restrict_to_render_frame,
      base::Bind(&DispatchOnConnectToScriptContext, target_port_id,
                 channel_name, &source, info, tls_channel_id, &port_created));
  // Note: |restrict_to_render_frame| may have been deleted at this point!

  if (port_created) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_OpenMessagePort(routing_id, target_port_id));
  } else {
    content::RenderThread::Get()->Send(new ExtensionHostMsg_CloseMessagePort(
        routing_id, target_port_id, false));
  }
}

void JSRendererMessagingService::DeliverMessage(
    const ScriptContextSet& context_set,
    const PortId& target_port_id,
    const Message& message,
    content::RenderFrame* restrict_to_render_frame) {
  context_set.ForEach(
      restrict_to_render_frame,
      base::Bind(&DeliverMessageToScriptContext, message, target_port_id));
}

void JSRendererMessagingService::DispatchOnDisconnect(
    const ScriptContextSet& context_set,
    const PortId& port_id,
    const std::string& error_message,
    content::RenderFrame* restrict_to_render_frame) {
  context_set.ForEach(
      restrict_to_render_frame,
      base::Bind(&DispatchOnDisconnectToScriptContext, port_id, error_message));
}

}  // namespace extensions
