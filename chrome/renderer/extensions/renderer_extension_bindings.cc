// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/renderer_extension_bindings.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "grit/renderer_resources.h"

using bindings_utils::GetStringResource;
using bindings_utils::ExtensionBase;

// Message passing API example (in a content script):
// var extension =
//    new chrome.Extension('00123456789abcdef0123456789abcdef0123456');
// var port = extension.connect();
// port.postMessage('Can you hear me now?');
// port.onmessage.addListener(function(msg, port) {
//   alert('response=' + msg);
//   port.postMessage('I got your reponse');
// });

namespace {

struct ExtensionData {
  struct PortData {
    int ref_count;  // how many contexts have a handle to this port
    bool disconnected;  // true if this port was forcefully disconnected
    PortData() : ref_count(0), disconnected(false) {}
  };
  std::map<int, PortData> ports;  // port ID -> data
};

static base::LazyInstance<ExtensionData> g_extension_data(
    base::LINKER_INITIALIZED);

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
const char* kExtensionDeps[] = { EventBindings::kName };

class ExtensionImpl : public ExtensionBase {
 public:
  ExtensionImpl()
      : ExtensionBase(RendererExtensionBindings::kName,
                      GetStringResource(IDR_RENDERER_EXTENSION_BINDINGS_JS),
                      arraysize(kExtensionDeps), kExtensionDeps) {
  }
  ~ExtensionImpl() {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("OpenChannelToExtension"))) {
      return v8::FunctionTemplate::New(OpenChannelToExtension);
    } else if (name->Equals(v8::String::New("PostMessage"))) {
      return v8::FunctionTemplate::New(PostMessage);
    } else if (name->Equals(v8::String::New("CloseChannel"))) {
      return v8::FunctionTemplate::New(CloseChannel);
    } else if (name->Equals(v8::String::New("PortAddRef"))) {
      return v8::FunctionTemplate::New(PortAddRef);
    } else if (name->Equals(v8::String::New("PortRelease"))) {
      return v8::FunctionTemplate::New(PortRelease);
    } else if (name->Equals(v8::String::New("GetL10nMessage"))) {
      return v8::FunctionTemplate::New(GetL10nMessage);
    }
    return ExtensionBase::GetNativeFunction(name);
  }

  // Creates a new messaging channel to the given extension.
  static v8::Handle<v8::Value> OpenChannelToExtension(
      const v8::Arguments& args) {
    // Get the current RenderView so that we can send a routed IPC message from
    // the correct source.
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
    if (!renderview)
      return v8::Undefined();

    if (args.Length() >= 3 && args[0]->IsString() && args[1]->IsString() &&
        args[2]->IsString()) {
      std::string source_id = *v8::String::Utf8Value(args[0]->ToString());
      std::string target_id = *v8::String::Utf8Value(args[1]->ToString());
      std::string channel_name = *v8::String::Utf8Value(args[2]->ToString());
      int port_id = -1;
      renderview->Send(new ViewHostMsg_OpenChannelToExtension(
          renderview->routing_id(), source_id, target_id,
          channel_name, &port_id));
      return v8::Integer::New(port_id);
    }
    return v8::Undefined();
  }

  // Sends a message along the given channel.
  static v8::Handle<v8::Value> PostMessage(const v8::Arguments& args) {
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
    if (!renderview)
      return v8::Undefined();

    if (args.Length() >= 2 && args[0]->IsInt32() && args[1]->IsString()) {
      int port_id = args[0]->Int32Value();
      if (!HasPortData(port_id)) {
        return v8::ThrowException(v8::Exception::Error(
          v8::String::New(kPortClosedError)));
      }
      std::string message = *v8::String::Utf8Value(args[1]->ToString());
      renderview->Send(new ViewHostMsg_ExtensionPostMessage(
          renderview->routing_id(), port_id, message));
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
        EventBindings::GetRenderThread()->Send(
            new ViewHostMsg_ExtensionCloseChannel(port_id));
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
        EventBindings::GetRenderThread()->Send(
            new ViewHostMsg_ExtensionCloseChannel(port_id));
        ClearPortData(port_id);
      }
    }
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> GetL10nMessage(const v8::Arguments& args) {
    if (args.Length() != 3 || !args[0]->IsString()) {
      NOTREACHED() << "Bad arguments";
      return v8::Undefined();
    }

    std::string extension_id;
    if (args[2]->IsNull() || !args[2]->IsString()) {
      return v8::Undefined();
    } else {
      extension_id = *v8::String::Utf8Value(args[2]->ToString());
      if (extension_id.empty())
        return v8::Undefined();
    }

    L10nMessagesMap* l10n_messages = GetL10nMessagesMap(extension_id);
    if (!l10n_messages) {
      // Get the current RenderView so that we can send a routed IPC message
      // from the correct source.
      RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
      if (!renderview)
        return v8::Undefined();

      L10nMessagesMap messages;
      // A sync call to load message catalogs for current extension.
      renderview->Send(new ViewHostMsg_GetExtensionMessageBundle(
          extension_id, &messages));

      // Save messages we got.
      ExtensionToL10nMessagesMap& l10n_messages_map =
          *GetExtensionToL10nMessagesMap();
      l10n_messages_map[extension_id] = messages;

      l10n_messages = GetL10nMessagesMap(extension_id);
    }

    std::string message_name = *v8::String::AsciiValue(args[0]);
    std::string message =
        ExtensionMessageBundle::GetL10nMessage(message_name, *l10n_messages);

    std::vector<std::string> substitutions;
    if (args[1]->IsNull() || args[1]->IsUndefined()) {
      // chrome.i18n.getMessage("message_name");
      // chrome.i18n.getMessage("message_name", null);
      return v8::String::New(message.c_str());
    } else if (args[1]->IsString()) {
      // chrome.i18n.getMessage("message_name", "one param");
      std::string substitute = *v8::String::Utf8Value(args[1]->ToString());
      substitutions.push_back(substitute);
    } else if (args[1]->IsArray()) {
      // chrome.i18n.getMessage("message_name", ["more", "params"]);
      v8::Array* placeholders = static_cast<v8::Array*>(*args[1]);
      uint32_t count = placeholders->Length();
      if (count <= 0 || count > 9)
        return v8::Undefined();
      for (uint32_t i = 0; i < count; ++i) {
        std::string substitute =
            *v8::String::Utf8Value(
                placeholders->Get(v8::Integer::New(i))->ToString());
        substitutions.push_back(substitute);
      }
    } else {
      NOTREACHED() << "Couldn't parse second parameter.";
      return v8::Undefined();
    }

    return v8::String::New(ReplaceStringPlaceholders(
        message, substitutions, NULL).c_str());
  }
};

// Convert a ListValue to a vector of V8 values.
static std::vector< v8::Handle<v8::Value> > ListValueToV8(
    const ListValue& value) {
  std::vector< v8::Handle<v8::Value> > v8_values;

  for (size_t i = 0; i < value.GetSize(); ++i) {
    Value* elem = NULL;
    value.Get(i, &elem);
    switch (elem->GetType()) {
      case Value::TYPE_NULL:
        v8_values.push_back(v8::Null());
        break;
      case Value::TYPE_BOOLEAN: {
        bool val;
        elem->GetAsBoolean(&val);
        v8_values.push_back(v8::Boolean::New(val));
        break;
      }
      case Value::TYPE_INTEGER: {
        int val;
        elem->GetAsInteger(&val);
        v8_values.push_back(v8::Integer::New(val));
        break;
      }
      case Value::TYPE_DOUBLE: {
        double val;
        elem->GetAsDouble(&val);
        v8_values.push_back(v8::Number::New(val));
        break;
      }
      case Value::TYPE_STRING: {
        std::string val;
        elem->GetAsString(&val);
        v8_values.push_back(v8::String::New(val.c_str()));
        break;
      }
      default:
        NOTREACHED() << "Unsupported Value type.";
        break;
    }
  }

  return v8_values;
}

}  // namespace

const char* RendererExtensionBindings::kName =
    "chrome/RendererExtensionBindings";

v8::Extension* RendererExtensionBindings::Get() {
  static v8::Extension* extension = new ExtensionImpl();
  return extension;
}

void RendererExtensionBindings::Invoke(const std::string& extension_id,
                                       const std::string& function_name,
                                       const ListValue& args,
                                       RenderView* renderview,
                                       const GURL& event_url) {
  v8::HandleScope handle_scope;
  std::vector< v8::Handle<v8::Value> > argv = ListValueToV8(args);
  EventBindings::CallFunction(extension_id,
                              function_name,
                              argv.size(),
                              &argv[0],
                              renderview,
                              event_url);
}
