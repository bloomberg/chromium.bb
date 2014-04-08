// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/messaging_bindings.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chrome/common/extensions/manifest_handlers/externally_connectable.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/scoped_persistent.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebScopedWindowFocusAllowedIndicator.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "v8/include/v8.h"

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

base::LazyInstance<ExtensionData> g_extension_data =
    LAZY_INSTANCE_INITIALIZER;

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

class ExtensionImpl : public ChromeV8Extension {
 public:
  ExtensionImpl(Dispatcher* dispatcher, ChromeV8Context* context)
      : ChromeV8Extension(dispatcher, context) {
    RouteFunction("CloseChannel",
        base::Bind(&ExtensionImpl::CloseChannel, base::Unretained(this)));
    RouteFunction("PortAddRef",
        base::Bind(&ExtensionImpl::PortAddRef, base::Unretained(this)));
    RouteFunction("PortRelease",
        base::Bind(&ExtensionImpl::PortRelease, base::Unretained(this)));
    RouteFunction("PostMessage",
        base::Bind(&ExtensionImpl::PostMessage, base::Unretained(this)));
    // TODO(fsamuel, kalman): Move BindToGC out of messaging natives.
    RouteFunction("BindToGC",
        base::Bind(&ExtensionImpl::BindToGC, base::Unretained(this)));
  }

  virtual ~ExtensionImpl() {}

  void ClearPortDataAndNotifyDispatcher(int port_id) {
    ClearPortData(port_id);
    dispatcher()->ClearPortData(port_id);
  }

  // Sends a message along the given channel.
  void PostMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
    content::RenderView* renderview = GetRenderView();
    if (!renderview)
      return;

    // Arguments are (int32 port_id, string message).
    CHECK(args.Length() == 2 &&
          args[0]->IsInt32() &&
          args[1]->IsString());

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
};

}  // namespace

ChromeV8Extension* MessagingBindings::Get(
    Dispatcher* dispatcher,
    ChromeV8Context* context) {
  return new ExtensionImpl(dispatcher, context);
}

// static
void MessagingBindings::DispatchOnConnect(
    const ChromeV8ContextSet::ContextSet& contexts,
    int target_port_id,
    const std::string& channel_name,
    const base::DictionaryValue& source_tab,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const GURL& source_url,
    const std::string& tls_channel_id,
    content::RenderView* restrict_to_render_view) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());

  bool port_created = false;
  std::string source_url_spec = source_url.spec();

  // TODO(kalman): pass in the full ChromeV8ContextSet; call ForEach.
  for (ChromeV8ContextSet::ContextSet::const_iterator it = contexts.begin();
       it != contexts.end(); ++it) {
    if (restrict_to_render_view &&
        restrict_to_render_view != (*it)->GetRenderView()) {
      continue;
    }

    // TODO(kalman): remove when ContextSet::ForEach is available.
    if ((*it)->v8_context().IsEmpty())
      continue;

    v8::Handle<v8::Value> tab = v8::Null(isolate);
    v8::Handle<v8::Value> tls_channel_id_value = v8::Undefined(isolate);
    const Extension* extension = (*it)->extension();
    if (extension) {
      if (!source_tab.empty() && !extension->is_platform_app())
        tab = converter->ToV8Value(&source_tab, (*it)->v8_context());

      ExternallyConnectableInfo* externally_connectable =
          ExternallyConnectableInfo::Get(extension);
      if (externally_connectable &&
          externally_connectable->accepts_tls_channel_id) {
        tls_channel_id_value =
            v8::String::NewFromUtf8(isolate,
                                    tls_channel_id.c_str(),
                                    v8::String::kNormalString,
                                    tls_channel_id.size());
      }
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
                              source_extension_id.c_str(),
                              v8::String::kNormalString,
                              source_extension_id.size()),
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

    v8::Handle<v8::Value> retval = (*it)->module_system()->CallModuleMethod(
        "messaging",
        "dispatchOnConnect",
        arraysize(arguments), arguments);

    if (retval.IsEmpty()) {
      LOG(ERROR) << "Empty return value from dispatchOnConnect.";
      continue;
    }

    CHECK(retval->IsBoolean());
    port_created |= retval->BooleanValue();
  }

  // If we didn't create a port, notify the other end of the channel (treat it
  // as a disconnect).
  if (!port_created) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_CloseChannel(
            target_port_id, kReceivingEndDoesntExistError));
  }
}

// static
void MessagingBindings::DeliverMessage(
    const ChromeV8ContextSet::ContextSet& contexts,
    int target_port_id,
    const Message& message,
    content::RenderView* restrict_to_render_view) {
  scoped_ptr<blink::WebScopedUserGesture> web_user_gesture;
  scoped_ptr<blink::WebScopedWindowFocusAllowedIndicator> allow_window_focus;
  if (message.user_gesture) {
    web_user_gesture.reset(new blink::WebScopedUserGesture);
    allow_window_focus.reset(new blink::WebScopedWindowFocusAllowedIndicator);
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  // TODO(kalman): pass in the full ChromeV8ContextSet; call ForEach.
  for (ChromeV8ContextSet::ContextSet::const_iterator it = contexts.begin();
       it != contexts.end(); ++it) {
    if (restrict_to_render_view &&
        restrict_to_render_view != (*it)->GetRenderView()) {
      continue;
    }

    // TODO(kalman): remove when ContextSet::ForEach is available.
    if ((*it)->v8_context().IsEmpty())
      continue;

    // Check to see whether the context has this port before bothering to create
    // the message.
    v8::Handle<v8::Value> port_id_handle =
        v8::Integer::New(isolate, target_port_id);
    v8::Handle<v8::Value> has_port = (*it)->module_system()->CallModuleMethod(
        "messaging",
        "hasPort",
        1, &port_id_handle);

    CHECK(!has_port.IsEmpty());
    if (!has_port->BooleanValue())
      continue;

    std::vector<v8::Handle<v8::Value> > arguments;
    arguments.push_back(v8::String::NewFromUtf8(isolate,
                                                message.data.c_str(),
                                                v8::String::kNormalString,
                                                message.data.size()));
    arguments.push_back(port_id_handle);
    (*it)->module_system()->CallModuleMethod("messaging",
                                             "dispatchOnMessage",
                                             &arguments);
  }
}

// static
void MessagingBindings::DispatchOnDisconnect(
    const ChromeV8ContextSet::ContextSet& contexts,
    int port_id,
    const std::string& error_message,
    content::RenderView* restrict_to_render_view) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  // TODO(kalman): pass in the full ChromeV8ContextSet; call ForEach.
  for (ChromeV8ContextSet::ContextSet::const_iterator it = contexts.begin();
       it != contexts.end(); ++it) {
    if (restrict_to_render_view &&
        restrict_to_render_view != (*it)->GetRenderView()) {
      continue;
    }

    // TODO(kalman): remove when ContextSet::ForEach is available.
    if ((*it)->v8_context().IsEmpty())
      continue;

    std::vector<v8::Handle<v8::Value> > arguments;
    arguments.push_back(v8::Integer::New(isolate, port_id));
    if (!error_message.empty()) {
      arguments.push_back(
          v8::String::NewFromUtf8(isolate, error_message.c_str()));
    } else {
      arguments.push_back(v8::Null(isolate));
    }
    (*it)->module_system()->CallModuleMethod("messaging",
                                             "dispatchOnDisconnect",
                                             &arguments);
  }
}

}  // namespace extensions
