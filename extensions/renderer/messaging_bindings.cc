// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/messaging_bindings.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/event_bindings.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/scoped_persistent.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebScopedWindowFocusAllowedIndicator.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "v8/include/v8.h"

// TODO(thestig): Remove #ifdef from this file when extensions are no longer
// used on mobile.
#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/manifest_handlers/externally_connectable.h"
#endif

// Message passing API example (in a content script):
// var extension =
//    new chrome.Extension('00123456789abcdef0123456789abcdef0123456');
// var port = runtime.connect();
// port.postMessage('Can you hear me now?');
// port.onmessage.addListener(function(msg, port) {
//   alert('response=' + msg);
//   port.postMessage('I got your reponse');
// });

using content::RenderThread;
using content::V8ValueConverter;

namespace extensions {

namespace {

struct ExtensionData {
  struct PortData {
    int ref_count;  // how many contexts have a handle to this port
    PortData() : ref_count(0) {}
  };
  std::map<int, PortData> ports;  // port ID -> data
};

base::LazyInstance<ExtensionData> g_extension_data = LAZY_INSTANCE_INITIALIZER;

bool HasPortData(int port_id) {
  return g_extension_data.Get().ports.find(port_id) !=
         g_extension_data.Get().ports.end();
}

ExtensionData::PortData& GetPortData(int port_id) {
  return g_extension_data.Get().ports[port_id];
}

void ClearPortData(int port_id) {
  g_extension_data.Get().ports.erase(port_id);
}

const char kPortClosedError[] = "Attempting to use a disconnected port object";
const char kReceivingEndDoesntExistError[] =
    "Could not establish connection. Receiving end does not exist.";

class ExtensionImpl : public ObjectBackedNativeHandler {
 public:
  ExtensionImpl(Dispatcher* dispatcher, ScriptContext* context)
      : ObjectBackedNativeHandler(context), dispatcher_(dispatcher) {
    RouteFunction(
        "CloseChannel",
        base::Bind(&ExtensionImpl::CloseChannel, base::Unretained(this)));
    RouteFunction(
        "PortAddRef",
        base::Bind(&ExtensionImpl::PortAddRef, base::Unretained(this)));
    RouteFunction(
        "PortRelease",
        base::Bind(&ExtensionImpl::PortRelease, base::Unretained(this)));
    RouteFunction(
        "PostMessage",
        base::Bind(&ExtensionImpl::PostMessage, base::Unretained(this)));
    // TODO(fsamuel, kalman): Move BindToGC out of messaging natives.
    RouteFunction("BindToGC",
                  base::Bind(&ExtensionImpl::BindToGC, base::Unretained(this)));
  }

  virtual ~ExtensionImpl() {}

 private:
  void ClearPortDataAndNotifyDispatcher(int port_id) {
    ClearPortData(port_id);
    dispatcher_->ClearPortData(port_id);
  }

  // Sends a message along the given channel.
  void PostMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
    content::RenderView* renderview = context()->GetRenderView();
    if (!renderview)
      return;

    // Arguments are (int32 port_id, string message).
    CHECK(args.Length() == 2 && args[0]->IsInt32() && args[1]->IsString());

    int port_id = args[0]->Int32Value();
    if (!HasPortData(port_id)) {
      args.GetIsolate()->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(args.GetIsolate(), kPortClosedError)));
      return;
    }

    renderview->Send(new ExtensionHostMsg_PostMessage(
        renderview->GetRoutingID(), port_id,
        Message(*v8::String::Utf8Value(args[1]),
                blink::WebUserGestureIndicator::isProcessingUserGesture())));
  }

  // Forcefully disconnects a port.
  void CloseChannel(const v8::FunctionCallbackInfo<v8::Value>& args) {
    // Arguments are (int32 port_id, boolean notify_browser).
    CHECK_EQ(2, args.Length());
    CHECK(args[0]->IsInt32());
    CHECK(args[1]->IsBoolean());

    int port_id = args[0]->Int32Value();
    if (!HasPortData(port_id))
      return;

    // Send via the RenderThread because the RenderView might be closing.
    bool notify_browser = args[1]->BooleanValue();
    if (notify_browser) {
      content::RenderThread::Get()->Send(
          new ExtensionHostMsg_CloseChannel(port_id, std::string()));
    }

    ClearPortDataAndNotifyDispatcher(port_id);
  }

  // A new port has been created for a context.  This occurs both when script
  // opens a connection, and when a connection is opened to this script.
  void PortAddRef(const v8::FunctionCallbackInfo<v8::Value>& args) {
    // Arguments are (int32 port_id).
    CHECK_EQ(1, args.Length());
    CHECK(args[0]->IsInt32());

    int port_id = args[0]->Int32Value();
    ++GetPortData(port_id).ref_count;
  }

  // The frame a port lived in has been destroyed.  When there are no more
  // frames with a reference to a given port, we will disconnect it and notify
  // the other end of the channel.
  void PortRelease(const v8::FunctionCallbackInfo<v8::Value>& args) {
    // Arguments are (int32 port_id).
    CHECK_EQ(1, args.Length());
    CHECK(args[0]->IsInt32());

    int port_id = args[0]->Int32Value();
    if (HasPortData(port_id) && --GetPortData(port_id).ref_count == 0) {
      // Send via the RenderThread because the RenderView might be closing.
      content::RenderThread::Get()->Send(
          new ExtensionHostMsg_CloseChannel(port_id, std::string()));
      ClearPortDataAndNotifyDispatcher(port_id);
    }
  }

  // Holds a |callback| to run sometime after |object| is GC'ed. |callback| will
  // not be executed re-entrantly to avoid running JS in an unexpected state.
  class GCCallback {
   public:
    static void Bind(v8::Handle<v8::Object> object,
                     v8::Handle<v8::Function> callback,
                     v8::Isolate* isolate) {
      GCCallback* cb = new GCCallback(object, callback, isolate);
      cb->object_.SetWeak(cb, NearDeathCallback);
    }

   private:
    static void NearDeathCallback(
        const v8::WeakCallbackData<v8::Object, GCCallback>& data) {
      // v8 says we need to explicitly reset weak handles from their callbacks.
      // It's not implicit as one might expect.
      data.GetParameter()->object_.reset();
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&GCCallback::RunCallback,
                     base::Owned(data.GetParameter())));
    }

    GCCallback(v8::Handle<v8::Object> object,
               v8::Handle<v8::Function> callback,
               v8::Isolate* isolate)
        : object_(object), callback_(callback), isolate_(isolate) {}

    void RunCallback() {
      v8::HandleScope handle_scope(isolate_);
      v8::Handle<v8::Function> callback = callback_.NewHandle(isolate_);
      v8::Handle<v8::Context> context = callback->CreationContext();
      if (context.IsEmpty())
        return;
      v8::Context::Scope context_scope(context);
      blink::WebScopedMicrotaskSuppression suppression;
      callback->Call(context->Global(), 0, NULL);
    }

    ScopedPersistent<v8::Object> object_;
    ScopedPersistent<v8::Function> callback_;
    v8::Isolate* isolate_;

    DISALLOW_COPY_AND_ASSIGN(GCCallback);
  };

  // void BindToGC(object, callback)
  //
  // Binds |callback| to be invoked *sometime after* |object| is garbage
  // collected. We don't call the method re-entrantly so as to avoid executing
  // JS in some bizarro undefined mid-GC state.
  void BindToGC(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.Length() == 2 && args[0]->IsObject() && args[1]->IsFunction());
    GCCallback::Bind(args[0].As<v8::Object>(),
                     args[1].As<v8::Function>(),
                     args.GetIsolate());
  }

  // Dispatcher handle. Not owned.
  Dispatcher* dispatcher_;
};

void DispatchOnConnectToScriptContext(
    int target_port_id,
    const std::string& channel_name,
    const base::DictionaryValue* source_tab,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id,
    bool* port_created,
    ScriptContext* script_context) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());

  const std::string& source_url_spec = info.source_url.spec();
  std::string target_extension_id = script_context->GetExtensionID();
  const Extension* extension = script_context->extension();

  v8::Handle<v8::Value> tab = v8::Null(isolate);
  v8::Handle<v8::Value> tls_channel_id_value = v8::Undefined(isolate);

  if (extension) {
    if (!source_tab->empty() && !extension->is_platform_app())
      tab = converter->ToV8Value(source_tab, script_context->v8_context());

#if defined(ENABLE_EXTENSIONS)
    ExternallyConnectableInfo* externally_connectable =
        ExternallyConnectableInfo::Get(extension);
    if (externally_connectable &&
        externally_connectable->accepts_tls_channel_id) {
      tls_channel_id_value = v8::String::NewFromUtf8(isolate,
                                                     tls_channel_id.c_str(),
                                                     v8::String::kNormalString,
                                                     tls_channel_id.size());
    }
#endif
  }

  v8::Handle<v8::Value> arguments[] = {
      // portId
      v8::Integer::New(isolate, target_port_id),
      // channelName
      v8::String::NewFromUtf8(isolate,
                              channel_name.c_str(),
                              v8::String::kNormalString,
                              channel_name.size()),
      // sourceTab
      tab,
      // sourceExtensionId
      v8::String::NewFromUtf8(isolate,
                              info.source_id.c_str(),
                              v8::String::kNormalString,
                              info.source_id.size()),
      // targetExtensionId
      v8::String::NewFromUtf8(isolate,
                              target_extension_id.c_str(),
                              v8::String::kNormalString,
                              target_extension_id.size()),
      // sourceUrl
      v8::String::NewFromUtf8(isolate,
                              source_url_spec.c_str(),
                              v8::String::kNormalString,
                              source_url_spec.size()),
      // tlsChannelId
      tls_channel_id_value,
  };

  v8::Handle<v8::Value> retval =
      script_context->module_system()->CallModuleMethod(
          "messaging", "dispatchOnConnect", arraysize(arguments), arguments);

  if (!retval.IsEmpty()) {
    CHECK(retval->IsBoolean());
    *port_created |= retval->BooleanValue();
  } else {
    LOG(ERROR) << "Empty return value from dispatchOnConnect.";
  }
}

void DeliverMessageToScriptContext(const std::string& message_data,
                                   int target_port_id,
                                   ScriptContext* script_context) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  // Check to see whether the context has this port before bothering to create
  // the message.
  v8::Handle<v8::Value> port_id_handle =
      v8::Integer::New(isolate, target_port_id);
  v8::Handle<v8::Value> has_port =
      script_context->module_system()->CallModuleMethod(
          "messaging", "hasPort", 1, &port_id_handle);

  CHECK(!has_port.IsEmpty());
  if (!has_port->BooleanValue())
    return;

  std::vector<v8::Handle<v8::Value> > arguments;
  arguments.push_back(v8::String::NewFromUtf8(isolate,
                                              message_data.c_str(),
                                              v8::String::kNormalString,
                                              message_data.size()));
  arguments.push_back(port_id_handle);
  script_context->module_system()->CallModuleMethod(
      "messaging", "dispatchOnMessage", &arguments);
}

void DispatchOnDisconnectToScriptContext(int port_id,
                                         const std::string& error_message,
                                         ScriptContext* script_context) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  std::vector<v8::Handle<v8::Value> > arguments;
  arguments.push_back(v8::Integer::New(isolate, port_id));
  if (!error_message.empty()) {
    arguments.push_back(
        v8::String::NewFromUtf8(isolate, error_message.c_str()));
  } else {
    arguments.push_back(v8::Null(isolate));
  }

  script_context->module_system()->CallModuleMethod(
      "messaging", "dispatchOnDisconnect", &arguments);
}

}  // namespace

ObjectBackedNativeHandler* MessagingBindings::Get(Dispatcher* dispatcher,
                                                  ScriptContext* context) {
  return new ExtensionImpl(dispatcher, context);
}

// static
void MessagingBindings::DispatchOnConnect(
    const ScriptContextSet& context_set,
    int target_port_id,
    const std::string& channel_name,
    const base::DictionaryValue& source_tab,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id,
    content::RenderView* restrict_to_render_view) {
  bool port_created = false;
  context_set.ForEach(info.target_id,
                      restrict_to_render_view,
                      base::Bind(&DispatchOnConnectToScriptContext,
                                 target_port_id,
                                 channel_name,
                                 &source_tab,
                                 info,
                                 tls_channel_id,
                                 &port_created));

  // If we didn't create a port, notify the other end of the channel (treat it
  // as a disconnect).
  if (!port_created) {
    content::RenderThread::Get()->Send(new ExtensionHostMsg_CloseChannel(
        target_port_id, kReceivingEndDoesntExistError));
  }
}

// static
void MessagingBindings::DeliverMessage(
    const ScriptContextSet& context_set,
    int target_port_id,
    const Message& message,
    content::RenderView* restrict_to_render_view) {
  scoped_ptr<blink::WebScopedUserGesture> web_user_gesture;
  scoped_ptr<blink::WebScopedWindowFocusAllowedIndicator> allow_window_focus;
  if (message.user_gesture) {
    web_user_gesture.reset(new blink::WebScopedUserGesture);
    allow_window_focus.reset(new blink::WebScopedWindowFocusAllowedIndicator);
  }

  context_set.ForEach(
      restrict_to_render_view,
      base::Bind(&DeliverMessageToScriptContext, message.data, target_port_id));
}

// static
void MessagingBindings::DispatchOnDisconnect(
    const ScriptContextSet& context_set,
    int port_id,
    const std::string& error_message,
    content::RenderView* restrict_to_render_view) {
  context_set.ForEach(
      restrict_to_render_view,
      base::Bind(&DispatchOnDisconnectToScriptContext, port_id, error_message));
}

}  // namespace extensions
