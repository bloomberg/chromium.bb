// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/event_bindings.h"

#include <vector>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/schema_generated_bindings.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/public/renderer/render_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;
using WebKit::WebSecurityOrigin;
using WebKit::WebURL;
using content::RenderThread;

namespace {

// A map of event names to the number of listeners for that event. We notify
// the browser about event listeners when we transition between 0 and 1.
typedef std::map<std::string, int> EventListenerCounts;

struct SingletonData {
  // A map of extension IDs to listener counts for that extension.
  std::map<std::string, EventListenerCounts> listener_counts_;
};

static base::LazyInstance<SingletonData> g_singleton_data =
    LAZY_INSTANCE_INITIALIZER;

static EventListenerCounts& GetListenerCounts(const std::string& extension_id) {
  return g_singleton_data.Get().listener_counts_[extension_id];
}

class ExtensionImpl : public ChromeV8Extension {
 public:
  explicit ExtensionImpl(ExtensionDispatcher* dispatcher)
      : ChromeV8Extension("extensions/event.js",
                          IDR_EVENT_BINDINGS_JS,
                          dispatcher) {
  }
  ~ExtensionImpl() {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("AttachEvent"))) {
      return v8::FunctionTemplate::New(AttachEvent, v8::External::New(this));
    } else if (name->Equals(v8::String::New("DetachEvent"))) {
      return v8::FunctionTemplate::New(DetachEvent, v8::External::New(this));
    }
    return ChromeV8Extension::GetNativeFunction(name);
  }

  // Attach an event name to an object.
  static v8::Handle<v8::Value> AttachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 1);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

    if (args[0]->IsString()) {
      ExtensionImpl* v8_extension = GetFromArguments<ExtensionImpl>(args);
      const ChromeV8ContextSet& context_set =
          v8_extension->extension_dispatcher()->v8_context_set();
      ChromeV8Context* context = context_set.GetCurrent();
      CHECK(context);
      EventListenerCounts& listener_counts =
          GetListenerCounts(context->extension_id());
      std::string event_name(*v8::String::AsciiValue(args[0]));

      if (!v8_extension->CheckCurrentContextAccessToExtensionAPI(event_name))
        return v8::Undefined();

      if (++listener_counts[event_name] == 1) {
        content::RenderThread::Get()->Send(
            new ExtensionHostMsg_AddListener(context->extension_id(),
                                             event_name));
      }
    }

    return v8::Undefined();
  }

  static v8::Handle<v8::Value> DetachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 1);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

    if (args[0]->IsString()) {
      ExtensionImpl* v8_extension = GetFromArguments<ExtensionImpl>(args);
      const ChromeV8ContextSet& context_set =
          v8_extension->extension_dispatcher()->v8_context_set();
      ChromeV8Context* context = context_set.GetCurrent();
      if (!context)
        return v8::Undefined();

      EventListenerCounts& listener_counts =
          GetListenerCounts(context->extension_id());
      std::string event_name(*v8::String::AsciiValue(args[0]));
      if (--listener_counts[event_name] == 0) {
        content::RenderThread::Get()->Send(
            new ExtensionHostMsg_RemoveListener(context->extension_id(),
                                                event_name));
      }
    }

    return v8::Undefined();
  }
};

}  // namespace

v8::Extension* EventBindings::Get(ExtensionDispatcher* dispatcher) {
  static v8::Extension* extension = new ExtensionImpl(dispatcher);
  return extension;
}
