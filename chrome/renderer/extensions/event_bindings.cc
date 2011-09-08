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
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_base.h"
#include "chrome/renderer/extensions/extension_bindings_context.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/js_only_v8_extensions.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_view.h"
#include "content/renderer/v8_value_converter.h"
#include "googleurl/src/gurl.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;
using WebKit::WebSecurityOrigin;
using WebKit::WebURL;

namespace {

// Keep a local cache of RenderThread so that we can mock it out for unit tests.
static RenderThreadBase* render_thread = NULL;

// A map of event names to the number of listeners for that event. We notify
// the browser about event listeners when we transition between 0 and 1.
typedef std::map<std::string, int> EventListenerCounts;

struct SingletonData {
  // A map of extension IDs to listener counts for that extension.
  std::map<std::string, EventListenerCounts> listener_counts_;
};

static base::LazyInstance<SingletonData> g_singleton_data(
    base::LINKER_INITIALIZED);

static EventListenerCounts& GetListenerCounts(const std::string& extension_id) {
  return g_singleton_data.Get().listener_counts_[extension_id];
}

class ExtensionImpl : public ExtensionBase {
 public:
  explicit ExtensionImpl(ExtensionDispatcher* dispatcher)
      : ExtensionBase(EventBindings::kName,
                      GetStringResource(IDR_EVENT_BINDINGS_JS),
                      0, NULL, dispatcher) {
  }
  ~ExtensionImpl() {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("AttachEvent"))) {
      return v8::FunctionTemplate::New(AttachEvent, v8::External::New(this));
    } else if (name->Equals(v8::String::New("DetachEvent"))) {
      return v8::FunctionTemplate::New(DetachEvent);
    } else if (name->Equals(v8::String::New("GetExternalFileEntry"))) {
      return v8::FunctionTemplate::New(GetExternalFileEntry);
    }
    return ExtensionBase::GetNativeFunction(name);
  }

  // Attach an event name to an object.
  static v8::Handle<v8::Value> AttachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 1);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

    if (args[0]->IsString()) {
      ExtensionBindingsContext* context =
          ExtensionBindingsContext::GetCurrent();
      CHECK(context);
      EventListenerCounts& listener_counts =
          GetListenerCounts(context->extension_id());
      std::string event_name(*v8::String::AsciiValue(args[0]));

      ExtensionImpl* v8_extension = GetFromArguments<ExtensionImpl>(args);
      if (!v8_extension->CheckPermissionForCurrentRenderView(event_name))
        return v8::Undefined();

      if (++listener_counts[event_name] == 1) {
        EventBindings::GetRenderThread()->Send(
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
      ExtensionBindingsContext* context =
          ExtensionBindingsContext::GetCurrent();
      if (!context)
        return v8::Undefined();

      EventListenerCounts& listener_counts =
          GetListenerCounts(context->extension_id());
      std::string event_name(*v8::String::AsciiValue(args[0]));
      if (--listener_counts[event_name] == 0) {
        EventBindings::GetRenderThread()->Send(
            new ExtensionHostMsg_RemoveListener(context->extension_id(),
                                                event_name));
      }
    }

    return v8::Undefined();
  }

  // Attach an event name to an object.
  static v8::Handle<v8::Value> GetExternalFileEntry(
      const v8::Arguments& args) {
  // TODO(zelidrag): Make this magic work on other platforms when file browser
  // matures enough on ChromeOS.
#if defined(OS_CHROMEOS)
    DCHECK(args.Length() == 1);
    DCHECK(args[0]->IsObject());
    v8::Local<v8::Object> file_def = args[0]->ToObject();
    std::string file_system_name(
        *v8::String::Utf8Value(file_def->Get(
            v8::String::New("fileSystemName"))));
    std::string file_system_path(
        *v8::String::Utf8Value(file_def->Get(
            v8::String::New("fileSystemRoot"))));
    std::string file_full_path(
        *v8::String::Utf8Value(file_def->Get(
            v8::String::New("fileFullPath"))));
    bool is_directory =
        file_def->Get(v8::String::New("fileIsDirectory"))->ToBoolean()->Value();
    WebFrame* webframe = WebFrame::frameForCurrentContext();
    return webframe->createFileEntry(
        WebKit::WebFileSystem::TypeExternal,
        WebKit::WebString::fromUTF8(file_system_name.c_str()),
        WebKit::WebString::fromUTF8(file_system_path.c_str()),
        WebKit::WebString::fromUTF8(file_full_path.c_str()),
        is_directory);
#else
    return v8::Undefined();
#endif
  }
};

}  // namespace

const char* EventBindings::kName = "chrome/EventBindings";

v8::Extension* EventBindings::Get(ExtensionDispatcher* dispatcher) {
  static v8::Extension* extension = new ExtensionImpl(dispatcher);
  return extension;
}

// static
void EventBindings::SetRenderThread(RenderThreadBase* thread) {
  render_thread = thread;
}

// static
RenderThreadBase* EventBindings::GetRenderThread() {
  return render_thread ? render_thread : RenderThread::current();
}
