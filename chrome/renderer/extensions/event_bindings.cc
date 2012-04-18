// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/event_bindings.h"

#include <vector>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
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

// A map of event names to the number of contexts listening to that event.
// We notify the browser about event listeners when we transition between 0
// and 1.
typedef std::map<std::string, int> EventListenerCounts;

// A map of extension IDs to listener counts for that extension.
base::LazyInstance<std::map<std::string, EventListenerCounts> >
    g_listener_counts = LAZY_INSTANCE_INITIALIZER;

}  // namespace

EventBindings::EventBindings(ExtensionDispatcher* dispatcher)
    : ChromeV8Extension(dispatcher) {
  RouteFunction("AttachEvent", base::Bind(&EventBindings::AttachEvent,
                                          base::Unretained(this)));
  RouteFunction("DetachEvent", base::Bind(&EventBindings::DetachEvent,
                                          base::Unretained(this)));
}

v8::Handle<v8::Value> EventBindings::AttachEvent(const v8::Arguments& args) {
  DCHECK(args.Length() == 1);
  // TODO(erikkay) should enforce that event name is a string in the bindings
  DCHECK(args[0]->IsString() || args[0]->IsUndefined());

  if (args[0]->IsString()) {
    const ChromeV8ContextSet& context_set =
        extension_dispatcher()->v8_context_set();
    ChromeV8Context* context = context_set.GetCurrent();
    CHECK(context);
    std::string event_name(*v8::String::AsciiValue(args[0]));

    if (!extension_dispatcher()->CheckCurrentContextAccessToExtensionAPI(
            event_name))
      return v8::Undefined();

    std::string extension_id = context->GetExtensionID();
    EventListenerCounts& listener_counts =
        g_listener_counts.Get()[extension_id];
    if (++listener_counts[event_name] == 1) {
      content::RenderThread::Get()->Send(
          new ExtensionHostMsg_AddListener(extension_id, event_name));
    }

    // This is called the first time the page has added a listener. Since
    // the background page is the only lazy page, we know this is the first
    // time this listener has been registered.
    if (IsLazyBackgroundPage(context->extension())) {
      content::RenderThread::Get()->Send(
          new ExtensionHostMsg_AddLazyListener(extension_id, event_name));
    }
  }

  return v8::Undefined();
}

v8::Handle<v8::Value> EventBindings::DetachEvent(const v8::Arguments& args) {
  DCHECK(args.Length() == 2);
  // TODO(erikkay) should enforce that event name is a string in the bindings
  DCHECK(args[0]->IsString() || args[0]->IsUndefined());

  if (args[0]->IsString() && args[1]->IsBoolean()) {
    const ChromeV8ContextSet& context_set =
        extension_dispatcher()->v8_context_set();
    ChromeV8Context* context = context_set.GetCurrent();
    if (!context)
      return v8::Undefined();

    std::string extension_id = context->GetExtensionID();
    EventListenerCounts& listener_counts =
        g_listener_counts.Get()[extension_id];
    std::string event_name(*v8::String::AsciiValue(args[0]));
    bool is_manual = args[1]->BooleanValue();

    if (--listener_counts[event_name] == 0) {
      content::RenderThread::Get()->Send(
          new ExtensionHostMsg_RemoveListener(extension_id, event_name));
    }

    // DetachEvent is called when the last listener for the context is
    // removed. If the context is the background page, and it removes the
    // last listener manually, then we assume that it is no longer interested
    // in being awakened for this event.
    if (is_manual && IsLazyBackgroundPage(context->extension())) {
      content::RenderThread::Get()->Send(
          new ExtensionHostMsg_RemoveLazyListener(extension_id, event_name));
    }
  }

  return v8::Undefined();
}

bool EventBindings::IsLazyBackgroundPage(const Extension* extension) {
  content::RenderView* render_view = GetCurrentRenderView();
  if (!render_view)
    return false;

  ExtensionHelper* helper = ExtensionHelper::Get(render_view);
  return (extension && extension->has_lazy_background_page() &&
          helper->view_type() == chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
}
