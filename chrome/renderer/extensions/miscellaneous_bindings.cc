// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/miscellaneous_bindings.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

// Message passing API example (in a content script):
// var extension =
//    new chrome.Extension('00123456789abcdef0123456789abcdef0123456');
// var port = extension.connect();
// port.postMessage('Can you hear me now?');
// port.onmessage.addListener(function(msg, port) {
//   alert('response=' + msg);
//   port.postMessage('I got your reponse');
// });

using content::RenderThread;

namespace {

struct ExtensionData {
  struct PortData {
    int ref_count;  // how many contexts have a handle to this port
    bool disconnected;  // true if this port was forcefully disconnected
    PortData() : ref_count(0), disconnected(false) {}
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
const char* kExtensionDeps[] = { "extensions/event.js" };

class ExtensionImpl : public ChromeV8Extension {
 public:
  explicit ExtensionImpl(ExtensionDispatcher* dispatcher)
      : ChromeV8Extension("extensions/miscellaneous_bindings.js",
                          IDR_MISCELLANEOUS_BINDINGS_JS,
                          arraysize(kExtensionDeps), kExtensionDeps,
                          dispatcher) {
  }
  ~ExtensionImpl() {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("PostMessage"))) {
      return v8::FunctionTemplate::New(PostMessage);
    } else if (name->Equals(v8::String::New("CloseChannel"))) {
      return v8::FunctionTemplate::New(CloseChannel);
    } else if (name->Equals(v8::String::New("PortAddRef"))) {
      return v8::FunctionTemplate::New(PortAddRef);
    } else if (name->Equals(v8::String::New("PortRelease"))) {
      return v8::FunctionTemplate::New(PortRelease);
    } else if (name->Equals(v8::String::New("BindToGC"))) {
      return v8::FunctionTemplate::New(BindToGC);
    }
    return ChromeV8Extension::GetNativeFunction(name);
  }

  // Sends a message along the given channel.
  static v8::Handle<v8::Value> PostMessage(const v8::Arguments& args) {
    content::RenderView* renderview = GetCurrentRenderView();
    if (!renderview)
      return v8::Undefined();

    if (args.Length() >= 2 && args[0]->IsInt32() && args[1]->IsString()) {
      int port_id = args[0]->Int32Value();
      if (!HasPortData(port_id)) {
        return v8::ThrowException(v8::Exception::Error(
          v8::String::New(kPortClosedError)));
      }
      std::string message = *v8::String::Utf8Value(args[1]->ToString());
      renderview->Send(new ExtensionHostMsg_PostMessage(
          renderview->GetRoutingID(), port_id, message));
    }
    return v8::Undefined();
  }

  // Forcefully disconnects a port.
  static v8::Handle<v8::Value> CloseChannel(const v8::Arguments& args) {
    if (args.Length() >= 2 && args[0]->IsInt32() && args[1]->IsBoolean()) {
      int port_id = args[0]->Int32Value();
      if (!HasPortData(port_id)) {
        return v8::Undefined();
      }
      // Send via the RenderThread because the RenderView might be closing.
      bool notify_browser = args[1]->BooleanValue();
      if (notify_browser)
        content::RenderThread::Get()->Send(
            new ExtensionHostMsg_CloseChannel(port_id));
      ClearPortData(port_id);
    }
    return v8::Undefined();
  }

  // A new port has been created for a context.  This occurs both when script
  // opens a connection, and when a connection is opened to this script.
  static v8::Handle<v8::Value> PortAddRef(const v8::Arguments& args) {
    if (args.Length() >= 1 && args[0]->IsInt32()) {
      int port_id = args[0]->Int32Value();
      ++GetPortData(port_id).ref_count;
    }
    return v8::Undefined();
  }

  // The frame a port lived in has been destroyed.  When there are no more
  // frames with a reference to a given port, we will disconnect it and notify
  // the other end of the channel.
  static v8::Handle<v8::Value> PortRelease(const v8::Arguments& args) {
    if (args.Length() >= 1 && args[0]->IsInt32()) {
      int port_id = args[0]->Int32Value();
      if (HasPortData(port_id) && --GetPortData(port_id).ref_count == 0) {
        // Send via the RenderThread because the RenderView might be closing.
        content::RenderThread::Get()->Send(
            new ExtensionHostMsg_CloseChannel(port_id));
        ClearPortData(port_id);
      }
    }
    return v8::Undefined();
  }

  struct GCCallbackArgs {
    v8::Persistent<v8::Object> object;
    v8::Persistent<v8::Function> callback;
  };

  static void GCCallback(v8::Persistent<v8::Value> object, void* parameter) {
    v8::HandleScope handle_scope;
    GCCallbackArgs* args = reinterpret_cast<GCCallbackArgs*>(parameter);
    args->callback->Call(args->callback->CreationContext()->Global(), 0, NULL);
    args->callback.Dispose();
    args->object.Dispose();
    delete args;
  }

  // Binds a callback to be invoked when the given object is garbage collected.
  static v8::Handle<v8::Value> BindToGC(const v8::Arguments& args) {
    if (args.Length() == 2 && args[0]->IsObject() && args[1]->IsFunction()) {
      GCCallbackArgs* context = new GCCallbackArgs;
      context->callback = v8::Persistent<v8::Function>::New(
          v8::Handle<v8::Function>::Cast(args[1]));
      context->object = v8::Persistent<v8::Object>::New(
          v8::Handle<v8::Object>::Cast(args[0]));
      context->object.MakeWeak(context, GCCallback);
    } else {
      NOTREACHED();
    }
    return v8::Undefined();
  }
};

}  // namespace

namespace extensions {

v8::Extension* MiscellaneousBindings::Get(ExtensionDispatcher* dispatcher) {
  static v8::Extension* extension = new ExtensionImpl(dispatcher);
  return extension;
}

void MiscellaneousBindings::DeliverMessage(
    const ChromeV8ContextSet::ContextSet& contexts,
    int target_port_id,
    const std::string& message,
    content::RenderView* restrict_to_render_view) {
  v8::HandleScope handle_scope;

  for (ChromeV8ContextSet::ContextSet::const_iterator it = contexts.begin();
       it != contexts.end(); ++it) {

    if (restrict_to_render_view &&
        restrict_to_render_view != (*it)->GetRenderView()) {
      continue;
    }

    // Check to see whether the context has this port before bothering to create
    // the message.
    v8::Handle<v8::Value> port_id_handle = v8::Integer::New(target_port_id);
    v8::Handle<v8::Value> has_port;
    if (!(*it)->CallChromeHiddenMethod("Port.hasPort", 1, &port_id_handle,
                                       &has_port)) {
      continue;
    }

    CHECK(!has_port.IsEmpty());
    if (!has_port->BooleanValue())
      continue;

    std::vector<v8::Handle<v8::Value> > arguments;
    arguments.push_back(v8::String::New(message.c_str(), message.size()));
    arguments.push_back(port_id_handle);
    CHECK((*it)->CallChromeHiddenMethod("Port.dispatchOnMessage",
                                        arguments.size(),
                                        &arguments[0],
                                        NULL));
  }
}

}  // namespace extension
