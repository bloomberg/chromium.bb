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
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/message_bundle.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/scoped_persistent.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"
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

namespace {

struct ExtensionData {
  struct PortData {
    int ref_count;  // how many contexts have a handle to this port
    PortData() : ref_count(0) {}
  };
  std::map<int, PortData> ports;  // port ID -> data
};

static base::LazyInstance<ExtensionData> g_extension_data =
    LAZY_INSTANCE_INITIALIZER;

static bool HasPortData(int port_id) {
  return g_extension_data.Get().ports.find(port_id) !=
      g_extension_data.Get().ports.end();
}

static ExtensionData::PortData& GetPortData(int port_id) {
  return g_extension_data.Get().ports[port_id];
}

static void ClearPortData(int port_id) {
  g_extension_data.Get().ports.erase(port_id);
}

const char kPortClosedError[] = "Attempting to use a disconnected port object";
const char kReceivingEndDoesntExistError[] =
    "Could not establish connection. Receiving end does not exist.";

class ExtensionImpl : public extensions::ChromeV8Extension {
 public:
  explicit ExtensionImpl(extensions::Dispatcher* dispatcher,
                         extensions::ChromeV8Context* context)
      : extensions::ChromeV8Extension(dispatcher, context) {
    RouteFunction("CloseChannel",
        base::Bind(&ExtensionImpl::CloseChannel, base::Unretained(this)));
    RouteFunction("PortAddRef",
        base::Bind(&ExtensionImpl::PortAddRef, base::Unretained(this)));
    RouteFunction("PortRelease",
        base::Bind(&ExtensionImpl::PortRelease, base::Unretained(this)));
    RouteFunction("PostMessage",
        base::Bind(&ExtensionImpl::PostMessage, base::Unretained(this)));
    RouteFunction("BindToGC",
        base::Bind(&ExtensionImpl::BindToGC, base::Unretained(this)));
  }

  virtual ~ExtensionImpl() {}

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
      v8::ThrowException(v8::Exception::Error(
        v8::String::New(kPortClosedError)));
      return;
    }

    renderview->Send(new ExtensionHostMsg_PostMessage(
        renderview->GetRoutingID(), port_id, *v8::String::AsciiValue(args[1])));
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

    ClearPortData(port_id);
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
      ClearPortData(port_id);
    }
  }

  // Holds a |callback| to run sometime after |object| is GC'ed. |callback| will
  // not be executed re-entrantly to avoid running JS in an unexpected state.
  class GCCallback {
   public:
    static void Bind(v8::Handle<v8::Object> object,
                     v8::Handle<v8::Function> callback) {
      GCCallback* cb = new GCCallback(object, callback);
      cb->object_.MakeWeak(cb, NearDeathCallback);
    }

   private:
    static void NearDeathCallback(v8::Isolate* isolate,
                                  v8::Persistent<v8::Object>* object,
                                  GCCallback* self) {
      // v8 says we need to explicitly reset weak handles from their callbacks.
      // It's not implicit as one might expect.
      self->object_.reset();
      base::MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(&GCCallback::RunCallback, base::Owned(self)));
    }

    GCCallback(v8::Handle<v8::Object> object, v8::Handle<v8::Function> callback)
        : object_(object), callback_(callback) {
    }

    void RunCallback() {
      v8::HandleScope handle_scope;
      v8::Handle<v8::Context> context = callback_->CreationContext();
      if (context.IsEmpty())
        return;
      v8::Context::Scope context_scope(context);
      WebKit::WebScopedMicrotaskSuppression suppression;
      callback_->Call(context->Global(), 0, NULL);
    }

    extensions::ScopedPersistent<v8::Object> object_;
    extensions::ScopedPersistent<v8::Function> callback_;

    DISALLOW_COPY_AND_ASSIGN(GCCallback);
  };

  // void BindToGC(object, callback)
  //
  // Binds |callback| to be invoked *sometime after* |object| is garbage
  // collected. We don't call the method re-entrantly so as to avoid executing
  // JS in some bizarro undefined mid-GC state.
  void BindToGC(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.Length() == 2 && args[0]->IsObject() && args[1]->IsFunction());
    GCCallback::Bind(args[0].As<v8::Object>(), args[1].As<v8::Function>());
  }
};

}  // namespace

namespace extensions {

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
    content::RenderView* restrict_to_render_view) {
  v8::HandleScope handle_scope;

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

    v8::Handle<v8::Value> tab = v8::Null();
    if (!source_tab.empty())
      tab = converter->ToV8Value(&source_tab, (*it)->v8_context());

    v8::Handle<v8::Value> arguments[] = {
      v8::Integer::New(target_port_id),
      v8::String::New(channel_name.c_str(), channel_name.size()),
      tab,
      v8::String::New(source_extension_id.c_str(), source_extension_id.size()),
      v8::String::New(target_extension_id.c_str(), target_extension_id.size()),
      v8::String::New(source_url_spec.c_str(), source_url_spec.size())
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
    const std::string& message,
    content::RenderView* restrict_to_render_view) {
  v8::HandleScope handle_scope;

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
    v8::Handle<v8::Value> port_id_handle = v8::Integer::New(target_port_id);
    v8::Handle<v8::Value> has_port = (*it)->module_system()->CallModuleMethod(
        "messaging",
        "hasPort",
        1, &port_id_handle);

    CHECK(!has_port.IsEmpty());
    if (!has_port->BooleanValue())
      continue;

    std::vector<v8::Handle<v8::Value> > arguments;
    arguments.push_back(v8::String::New(message.c_str(), message.size()));
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
  v8::HandleScope handle_scope;

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
    arguments.push_back(v8::Integer::New(port_id));
    if (!error_message.empty()) {
      arguments.push_back(v8::String::New(error_message.c_str()));
    } else {
      arguments.push_back(v8::Null());
    }
    (*it)->module_system()->CallModuleMethod("messaging",
                                             "dispatchOnDisconnect",
                                             &arguments);
  }
}

}  // namespace extensions
