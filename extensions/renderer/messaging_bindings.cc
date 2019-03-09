// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/messaging_bindings.h"

#include <stdint.h>

#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_context.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/extension_bindings_system.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_port.h"
#include "extensions/renderer/gc_callback.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/messaging_util.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/v8_helpers.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "extensions/renderer/worker_thread_util.h"
#include "gin/converter.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "v8/include/v8.h"

// Message passing API example (in a content script):
// var port = runtime.connect();
// port.postMessage('Can you hear me now?');
// port.onmessage.addListener(function(msg, port) {
//   alert('response=' + msg);
//   port.postMessage('I got your reponse');
// });

namespace extensions {

using v8_helpers::ToV8String;

namespace {

// A global map between ScriptContext and MessagingBindings.
base::LazyInstance<std::map<ScriptContext*, MessagingBindings*>>::
    DestructorAtExit g_messaging_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

MessagingBindings::MessagingBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context),
      weak_ptr_factory_(this) {
  g_messaging_map.Get()[context] = this;
}

MessagingBindings::~MessagingBindings() {
  g_messaging_map.Get().erase(context());
  if (num_extension_ports_ > 0) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Extensions.Messaging.ExtensionPortsCreated.Total", next_js_id_);
  }
}

void MessagingBindings::AddRoutes() {
  RouteHandlerFunction("CloseChannel",
                       base::BindRepeating(&MessagingBindings::CloseChannel,
                                           base::Unretained(this)));
  RouteHandlerFunction("PostMessage",
                       base::BindRepeating(&MessagingBindings::PostMessage,
                                           base::Unretained(this)));
  // TODO(fsamuel, kalman): Move BindToGC out of messaging natives.
  RouteHandlerFunction("BindToGC",
                       base::BindRepeating(&MessagingBindings::BindToGC,
                                           base::Unretained(this)));
  RouteHandlerFunction(
      "OpenChannelToExtension", "runtime.connect",
      base::BindRepeating(&MessagingBindings::OpenChannelToExtension,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "OpenChannelToNativeApp", "runtime.connectNative",
      base::BindRepeating(&MessagingBindings::OpenChannelToNativeApp,
                          base::Unretained(this)));
  RouteHandlerFunction("OpenChannelToTab",
                       base::BindRepeating(&MessagingBindings::OpenChannelToTab,
                                           base::Unretained(this)));
}

// static
MessagingBindings* MessagingBindings::ForContext(ScriptContext* context) {
  return g_messaging_map.Get()[context];
}

ExtensionPort* MessagingBindings::GetPortWithId(const PortId& id) {
  for (const auto& key_value : ports_) {
    if (key_value.second->id() == id)
      return key_value.second.get();
  }
  return nullptr;
}

ExtensionPort* MessagingBindings::CreateNewPortWithId(const PortId& id) {
  int js_id = GetNextJsId();
  auto port = std::make_unique<ExtensionPort>(context(), id, js_id);
  return ports_.insert(std::make_pair(js_id, std::move(port)))
      .first->second.get();
}

void MessagingBindings::PostMessage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Arguments are (int32_t port_id, string message).
  CHECK(args.Length() == 2);
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsString());

  int js_port_id = args[0].As<v8::Int32>()->Value();
  auto iter = ports_.find(js_port_id);

  if (iter == ports_.end())
    return;

  ExtensionPort& port = *iter->second;

  v8::Isolate* isolate = args.GetIsolate();
  std::string error;
  std::unique_ptr<Message> message = messaging_util::MessageFromJSONString(
      isolate, args[1].As<v8::String>(), &error, context()->web_frame());
  if (!message) {
    args.GetReturnValue().Set(gin::StringToV8(isolate, error));
    return;
  }

  port.PostExtensionMessage(std::move(message));
}

void MessagingBindings::CloseChannel(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Arguments are (int32_t port_id, bool force_close).
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsBoolean());

  int js_port_id = args[0].As<v8::Int32>()->Value();
  bool force_close = args[1].As<v8::Boolean>()->Value();
  ClosePort(js_port_id, force_close);
}

void MessagingBindings::BindToGC(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 3);
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsFunction());
  CHECK(args[2]->IsInt32());
  int js_port_id = args[2].As<v8::Int32>()->Value();
  base::Closure fallback = base::DoNothing();
  if (js_port_id >= 0) {
    // TODO(robwu): Falling back to closing the port shouldn't be needed. If
    // the script context is destroyed, then the frame has navigated. But that
    // is already detected by the browser, so this logic is redundant. Remove
    // this fallback (and move BindToGC out of messaging because it is also
    // used in other places that have nothing to do with messaging...).
    fallback = base::Bind(&MessagingBindings::ClosePort,
                          weak_ptr_factory_.GetWeakPtr(), js_port_id,
                          false /* force_close */);
  }
  // Destroys itself when the object is GC'd or context is invalidated.
  new GCCallback(context(), args[0].As<v8::Object>(),
                 args[1].As<v8::Function>(), fallback);
}

void MessagingBindings::OpenChannelToExtension(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderFrame* render_frame = context()->GetRenderFrame();
  bool is_for_service_worker = false;
  if (!render_frame &&
      !(is_for_service_worker = worker_thread_util::IsWorkerThread()))
    return;

  // The Javascript code should validate/fill the arguments.
  CHECK_EQ(args.Length(), 3);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsBoolean());

  int js_id = GetNextJsId();
  PortId port_id(context()->context_id(), js_id, true);
  ports_[js_id] = std::make_unique<ExtensionPort>(context(), port_id, js_id);

  ExtensionMsg_ExternalConnectionInfo info;

  // For messaging APIs, hosted apps should be considered a web page so hide
  // its extension ID.
  const Extension* extension = context()->extension();
  if (extension && !extension->is_hosted_app()) {
    info.source_endpoint =
        context()->context_type() == Feature::CONTENT_SCRIPT_CONTEXT
            ? MessagingEndpoint::ForContentScript(extension->id())
            : MessagingEndpoint::ForExtension(extension->id());
  } else {
    info.source_endpoint = MessagingEndpoint::ForWebPage();
  }

  v8::Isolate* isolate = args.GetIsolate();
  info.target_id = *v8::String::Utf8Value(isolate, args[0]);

  info.source_url = context()->url();
  std::string channel_name = *v8::String::Utf8Value(isolate, args[1]);

  {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "Extensions.Messaging.SetPortIdTime.Extension");
    if (is_for_service_worker) {
      WorkerThreadDispatcher::GetBindingsSystem()
          ->GetIPCMessageSender()
          ->SendOpenMessageChannel(
              context(), port_id, MessageTarget::ForExtension(info.target_id),
              channel_name, false /* include_tls_channel_id */);
    } else {
      render_frame->Send(new ExtensionHostMsg_OpenChannelToExtension(
          PortContext::ForFrame(render_frame->GetRoutingID()), info,
          channel_name, port_id));
    }
  }

  ++num_extension_ports_;
  args.GetReturnValue().Set(static_cast<int32_t>(js_id));
}

void MessagingBindings::OpenChannelToNativeApp(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // The Javascript code should validate/fill the arguments.
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());
  // This should be checked by our function routing code.
  CHECK(context()->GetAvailability("runtime.connectNative").is_available());

  content::RenderFrame* render_frame = context()->GetRenderFrame();
  bool is_for_service_worker = false;
  if (!render_frame &&
      !(is_for_service_worker = worker_thread_util::IsWorkerThread()))
    return;

  std::string native_app_name =
      *v8::String::Utf8Value(args.GetIsolate(), args[0]);

  int js_id = GetNextJsId();
  PortId port_id(context()->context_id(), js_id, true);
  ports_[js_id] = std::make_unique<ExtensionPort>(context(), port_id, js_id);

  {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "Extensions.Messaging.SetPortIdTime.NativeApp");
    if (is_for_service_worker) {
      WorkerThreadDispatcher::GetBindingsSystem()
          ->GetIPCMessageSender()
          ->SendOpenMessageChannel(context(), port_id,
                                   MessageTarget::ForNativeApp(native_app_name),
                                   "", false /* include_tls_channel_id */);
    } else {
      render_frame->Send(new ExtensionHostMsg_OpenChannelToNativeApp(
          PortContext::ForFrame(render_frame->GetRoutingID()), native_app_name,
          port_id));
    }
  }

  args.GetReturnValue().Set(static_cast<int32_t>(js_id));
}

void MessagingBindings::OpenChannelToTab(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderFrame* render_frame = context()->GetRenderFrame();
  bool is_for_service_worker = false;
  if (!render_frame &&
      !(is_for_service_worker = worker_thread_util::IsWorkerThread()))
    return;

  DCHECK_NE(context()->context_type(), Feature::CONTENT_SCRIPT_CONTEXT);

  // tabs_custom_bindings.js unwraps arguments to tabs.connect/sendMessage and
  // passes them to OpenChannelToTab, in the following order:
  // - |tab_id| - Positive number that specifies the destination of the channel.
  // - |frame_id| - Target frame(s) in the tab where onConnect is dispatched:
  //   -1 for all frames, 0 for the main frame, >0 for a child frame.
  // - |extension_id| - ID of the initiating extension.
  // - |channel_name| - A user-defined channel name.
  CHECK(args.Length() == 4);
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsInt32());
  CHECK(args[2]->IsString());
  CHECK(args[3]->IsString());

  int js_id = GetNextJsId();
  PortId port_id(context()->context_id(), js_id, true);
  ports_[js_id] = std::make_unique<ExtensionPort>(context(), port_id, js_id);

  ExtensionMsg_TabTargetConnectionInfo info;
  info.tab_id = args[0].As<v8::Int32>()->Value();
  info.frame_id = args[1].As<v8::Int32>()->Value();
  // TODO(devlin): Why is this not part of info?
  v8::Isolate* isolate = args.GetIsolate();
  std::string extension_id = *v8::String::Utf8Value(isolate, args[2]);
  std::string channel_name = *v8::String::Utf8Value(isolate, args[3]);

  if (!is_for_service_worker) {
    ExtensionFrameHelper* frame_helper =
        ExtensionFrameHelper::Get(render_frame);
    DCHECK(frame_helper);
  }

  {
    SCOPED_UMA_HISTOGRAM_TIMER("Extensions.Messaging.SetPortIdTime.Tab");
    if (is_for_service_worker) {
      WorkerThreadDispatcher::GetBindingsSystem()
          ->GetIPCMessageSender()
          ->SendOpenMessageChannel(
              context(), port_id,
              MessageTarget::ForTab(info.tab_id, info.frame_id), channel_name,
              false /* include_tls_channel_id */);
    } else {
      render_frame->Send(new ExtensionHostMsg_OpenChannelToTab(
          PortContext::ForFrame(render_frame->GetRoutingID()), info,
          extension_id, channel_name, port_id));
    }
  }

  args.GetReturnValue().Set(static_cast<int32_t>(js_id));
}

void MessagingBindings::ClosePort(int js_port_id, bool force_close) {
  // TODO(robwu): Merge this logic with CloseChannel once the TODO in BindToGC
  // has been addressed.
  auto iter = ports_.find(js_port_id);
  if (iter != ports_.end()) {
    std::unique_ptr<ExtensionPort> port = std::move(iter->second);
    ports_.erase(iter);
    port->Close(force_close);
  }
}

int MessagingBindings::GetNextJsId() {
  return next_js_id_++;
}

}  // namespace extensions
