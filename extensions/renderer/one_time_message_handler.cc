// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/one_time_message_handler.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/supports_user_data.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/messaging_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"
#include "ipc/ipc_message.h"

namespace extensions {

namespace {

constexpr char kRuntimeOnMessage[] = "runtime.onMessage";
constexpr char kExtensionOnRequest[] = "extension.onRequest";

// An opener port in the context; i.e., the caller of runtime.sendMessage.
struct OneTimeOpener {
  int request_id = -1;
  int routing_id = MSG_ROUTING_NONE;
};

// A receiver port in the context; i.e., a listener to runtime.onMessage.
struct OneTimeReceiver {
  int routing_id = MSG_ROUTING_NONE;
  const char* event_name = nullptr;
  v8::Global<v8::Object> sender;
};

using OneTimeMessageCallback =
    base::OnceCallback<void(gin::Arguments* arguments)>;
struct OneTimeMessageContextData : public base::SupportsUserData::Data {
  std::map<PortId, OneTimeOpener> openers;
  std::map<PortId, OneTimeReceiver> receivers;
  std::vector<std::unique_ptr<OneTimeMessageCallback>> pending_callbacks;
};

constexpr char kExtensionOneTimeMessageContextData[] =
    "extension_one_time_message_context_data";

OneTimeMessageContextData* GetPerContextData(v8::Local<v8::Context> context,
                                             bool should_create) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data)
    return nullptr;
  auto* data = static_cast<OneTimeMessageContextData*>(
      per_context_data->GetUserData(kExtensionOneTimeMessageContextData));

  if (!data && should_create) {
    auto messaging_data = std::make_unique<OneTimeMessageContextData>();
    data = messaging_data.get();
    per_context_data->SetUserData(kExtensionOneTimeMessageContextData,
                                  std::move(messaging_data));
  }

  return data;
}

int RoutingIdForScriptContext(ScriptContext* script_context) {
  content::RenderFrame* render_frame = script_context->GetRenderFrame();
  return render_frame ? render_frame->GetRoutingID() : MSG_ROUTING_NONE;
}

void OneTimeMessageResponseHelper(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(info.Data()->IsExternal());

  gin::Arguments arguments(info);
  v8::Isolate* isolate = arguments.isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  OneTimeMessageContextData* data = GetPerContextData(context, false);
  if (!data)
    return;

  v8::Local<v8::External> external = info.Data().As<v8::External>();
  auto* raw_callback = static_cast<OneTimeMessageCallback*>(external->Value());
  auto iter = std::find_if(
      data->pending_callbacks.begin(), data->pending_callbacks.end(),
      [raw_callback](const std::unique_ptr<OneTimeMessageCallback>& callback) {
        return callback.get() == raw_callback;
      });
  if (iter == data->pending_callbacks.end())
    return;

  std::unique_ptr<OneTimeMessageCallback> callback = std::move(*iter);
  data->pending_callbacks.erase(iter);
  std::move(*callback).Run(&arguments);
}

}  // namespace

OneTimeMessageHandler::OneTimeMessageHandler(
    NativeExtensionBindingsSystem* bindings_system)
    : bindings_system_(bindings_system), weak_factory_(this) {}
OneTimeMessageHandler::~OneTimeMessageHandler() {}

bool OneTimeMessageHandler::HasPort(ScriptContext* script_context,
                                    const PortId& port_id) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  OneTimeMessageContextData* data =
      GetPerContextData(script_context->v8_context(), false);
  if (!data)
    return false;
  return port_id.is_opener ? base::ContainsKey(data->openers, port_id)
                           : base::ContainsKey(data->receivers, port_id);
}

void OneTimeMessageHandler::SendMessage(
    ScriptContext* script_context,
    const PortId& new_port_id,
    const std::string& target_id,
    const std::string& method_name,
    bool include_tls_channel_id,
    const Message& message,
    v8::Local<v8::Function> response_callback) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  DCHECK(new_port_id.is_opener);
  DCHECK_EQ(script_context->context_id(), new_port_id.context_id);

  OneTimeMessageContextData* data =
      GetPerContextData(script_context->v8_context(), true);
  DCHECK(data);

  bool wants_response = !response_callback.IsEmpty();
  int routing_id = RoutingIdForScriptContext(script_context);
  if (wants_response) {
    int request_id =
        bindings_system_->api_system()->request_handler()->AddPendingRequest(
            script_context->v8_context(), response_callback);
    OneTimeOpener& port = data->openers[new_port_id];
    port.request_id = request_id;
    port.routing_id = routing_id;
  }

  IPCMessageSender* ipc_sender = bindings_system_->GetIPCMessageSender();
  ipc_sender->SendOpenChannelToExtension(script_context, new_port_id, target_id,
                                         method_name, include_tls_channel_id);
  ipc_sender->SendPostMessageToPort(routing_id, new_port_id, message);

  if (!wants_response) {
    bool close_channel = true;
    ipc_sender->SendCloseMessagePort(routing_id, new_port_id, close_channel);
  }
}

void OneTimeMessageHandler::AddReceiver(ScriptContext* script_context,
                                        const PortId& target_port_id,
                                        v8::Local<v8::Object> sender,
                                        Event event) {
  DCHECK(!target_port_id.is_opener);
  DCHECK_NE(script_context->context_id(), target_port_id.context_id);

  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = script_context->v8_context();

  OneTimeMessageContextData* data = GetPerContextData(context, true);
  DCHECK(data);
  DCHECK(!base::ContainsKey(data->receivers, target_port_id));
  OneTimeReceiver& receiver = data->receivers[target_port_id];
  receiver.sender.Reset(isolate, sender);
  receiver.routing_id = RoutingIdForScriptContext(script_context);
  receiver.event_name =
      event == Event::ON_REQUEST ? kExtensionOnRequest : kRuntimeOnMessage;
}

bool OneTimeMessageHandler::DeliverMessage(ScriptContext* script_context,
                                           const Message& message,
                                           const PortId& target_port_id) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  return target_port_id.is_opener
             ? DeliverReplyToOpener(script_context, message, target_port_id)
             : DeliverMessageToReceiver(script_context, message,
                                        target_port_id);
}

bool OneTimeMessageHandler::Disconnect(ScriptContext* script_context,
                                       const PortId& port_id,
                                       const std::string& error_message) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  return port_id.is_opener
             ? DisconnectOpener(script_context, port_id, error_message)
             : DisconnectReceiver(script_context, port_id);
}

bool OneTimeMessageHandler::DeliverMessageToReceiver(
    ScriptContext* script_context,
    const Message& message,
    const PortId& target_port_id) {
  DCHECK(!target_port_id.is_opener);

  v8::Isolate* isolate = script_context->isolate();
  v8::Local<v8::Context> context = script_context->v8_context();

  bool handled = false;

  OneTimeMessageContextData* data = GetPerContextData(context, false);
  if (!data)
    return handled;

  auto iter = data->receivers.find(target_port_id);
  if (iter == data->receivers.end())
    return handled;

  handled = true;
  OneTimeReceiver& port = iter->second;

  // This port is a receiver, so we invoke the onMessage event and provide a
  // callback through which the port can respond. The port stays open until
  // we receive a response.
  // TODO(devlin): With chrome.runtime.sendMessage, we actually require that a
  // listener return `true` if they intend to respond asynchronously; otherwise
  // we close the port. With both sendMessage and sendRequest, we can monitor
  // the lifetime of the response callback and close the port if it's garbage-
  // collected.
  auto callback = std::make_unique<OneTimeMessageCallback>(
      base::Bind(&OneTimeMessageHandler::OnOneTimeMessageResponse,
                 weak_factory_.GetWeakPtr(), target_port_id));
  v8::Local<v8::External> external = v8::External::New(isolate, callback.get());
  v8::Local<v8::Function> response_function;

  if (!v8::Function::New(context, &OneTimeMessageResponseHelper, external)
           .ToLocal(&response_function)) {
    NOTREACHED();
    return handled;
  }

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> v8_message =
      messaging_util::MessageToV8(context, message);
  v8::Local<v8::Object> v8_sender = port.sender.Get(isolate);
  std::vector<v8::Local<v8::Value>> args = {v8_message, v8_sender,
                                            response_function};

  data->pending_callbacks.push_back(std::move(callback));
  bindings_system_->api_system()->event_handler()->FireEventInContext(
      port.event_name, context, &args, nullptr);

  return handled;
}

bool OneTimeMessageHandler::DeliverReplyToOpener(ScriptContext* script_context,
                                                 const Message& message,
                                                 const PortId& target_port_id) {
  DCHECK(target_port_id.is_opener);

  v8::Local<v8::Context> context = script_context->v8_context();
  bool handled = false;

  OneTimeMessageContextData* data = GetPerContextData(context, false);
  if (!data)
    return handled;

  auto iter = data->openers.find(target_port_id);
  if (iter == data->openers.end())
    return handled;

  handled = true;

  const OneTimeOpener& port = iter->second;
  DCHECK_NE(-1, port.request_id);

  // This port was the opener, so the message is the response from the
  // receiver. Invoke the callback and close the message port.
  v8::Local<v8::Value> v8_message =
      messaging_util::MessageToV8(context, message);
  std::vector<v8::Local<v8::Value>> args = {v8_message};
  bindings_system_->api_system()->request_handler()->CompleteRequest(
      port.request_id, args, std::string());

  bool close_channel = true;
  bindings_system_->GetIPCMessageSender()->SendCloseMessagePort(
      port.routing_id, target_port_id, close_channel);
  data->openers.erase(iter);

  return handled;
}

bool OneTimeMessageHandler::DisconnectReceiver(ScriptContext* script_context,
                                               const PortId& port_id) {
  v8::Local<v8::Context> context = script_context->v8_context();
  bool handled = false;

  OneTimeMessageContextData* data = GetPerContextData(context, false);
  if (!data)
    return handled;

  auto iter = data->receivers.find(port_id);
  if (iter == data->receivers.end())
    return handled;

  handled = true;
  data->receivers.erase(iter);
  return handled;
}

bool OneTimeMessageHandler::DisconnectOpener(ScriptContext* script_context,
                                             const PortId& port_id,
                                             const std::string& error_message) {
  bool handled = false;

  OneTimeMessageContextData* data =
      GetPerContextData(script_context->v8_context(), false);
  if (!data)
    return handled;

  auto iter = data->openers.find(port_id);
  if (iter == data->openers.end())
    return handled;

  handled = true;
  const OneTimeOpener& port = iter->second;
  DCHECK_NE(-1, port.request_id);

  bindings_system_->api_system()->request_handler()->CompleteRequest(
      port.request_id, std::vector<v8::Local<v8::Value>>(), error_message);

  data->openers.erase(iter);
  return handled;
}

void OneTimeMessageHandler::OnOneTimeMessageResponse(
    const PortId& port_id,
    gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  OneTimeMessageContextData* data = GetPerContextData(context, false);
  DCHECK(data);
  auto iter = data->receivers.find(port_id);
  DCHECK(iter != data->receivers.end());
  int routing_id = iter->second.routing_id;
  data->receivers.erase(iter);

  if (arguments->Length() < 1) {
    arguments->ThrowTypeError("Required argument 'message' is missing");
    return;
  }
  v8::Local<v8::Value> value;
  CHECK(arguments->GetNext(&value));

  std::unique_ptr<Message> message =
      messaging_util::MessageFromV8(context, value);
  if (!message) {
    arguments->ThrowTypeError("Illegal argument to Port.postMessage");
    return;
  }
  IPCMessageSender* ipc_sender = bindings_system_->GetIPCMessageSender();
  ipc_sender->SendPostMessageToPort(routing_id, port_id, *message);
  bool close_channel = true;
  ipc_sender->SendCloseMessagePort(routing_id, port_id, close_channel);
}

}  // namespace extensions
