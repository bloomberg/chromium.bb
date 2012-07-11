// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/event_bindings.h"

#include <vector>

#include "base/bind.h"
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/event_filter.h"
#include "chrome/common/extensions/value_counter.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/view_type.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
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
using extensions::Extension;

namespace {

// A map of event names to the number of contexts listening to that event.
// We notify the browser about event listeners when we transition between 0
// and 1.
typedef std::map<std::string, int> EventListenerCounts;

// A map of extension IDs to listener counts for that extension.
base::LazyInstance<std::map<std::string, EventListenerCounts> >
    g_listener_counts = LAZY_INSTANCE_INITIALIZER;

// A map of event names to a (filter -> count) map. The map is used to keep
// track of which filters are in effect for which events.
// We notify the browser about filtered event listeners when we transition
// between 0 and 1.
typedef std::map<std::string, linked_ptr<extensions::ValueCounter> >
    FilteredEventListenerCounts;

// A map of extension IDs to filtered listener counts for that extension.
base::LazyInstance<std::map<std::string, FilteredEventListenerCounts> >
    g_filtered_listener_counts = LAZY_INSTANCE_INITIALIZER;

// TODO(koz): Merge this into EventBindings.
class ExtensionImpl : public ChromeV8Extension {
 public:

  ExtensionImpl(ExtensionDispatcher* dispatcher,
      extensions::EventFilter* event_filter)
      : ChromeV8Extension(dispatcher),
        event_filter_(event_filter) {
    RouteFunction("AttachEvent",
        base::Bind(&ExtensionImpl::AttachEvent,
                   base::Unretained(this)));
    RouteFunction("DetachEvent",
        base::Bind(&ExtensionImpl::DetachEvent,
                   base::Unretained(this)));
    RouteFunction("AttachFilteredEvent",
        base::Bind(&ExtensionImpl::AttachFilteredEvent,
                   base::Unretained(this)));
    RouteFunction("DetachFilteredEvent",
        base::Bind(&ExtensionImpl::DetachFilteredEvent,
                   base::Unretained(this)));
    RouteFunction("MatchAgainstEventFilter",
        base::Bind(&ExtensionImpl::MatchAgainstEventFilter,
                   base::Unretained(this)));
  }
  ~ExtensionImpl() {}

  // Attach an event name to an object.
  v8::Handle<v8::Value> AttachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 1);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

    if (args[0]->IsString()) {
      std::string event_name = *v8::String::AsciiValue(args[0]->ToString());
      const ChromeV8ContextSet& context_set =
          extension_dispatcher()->v8_context_set();
      ChromeV8Context* context = context_set.GetCurrent();
      CHECK(context);

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

  v8::Handle<v8::Value> DetachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 2);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

    if (args[0]->IsString() && args[1]->IsBoolean()) {
      std::string event_name = *v8::String::AsciiValue(args[0]->ToString());
      bool is_manual = args[1]->BooleanValue();

      const ChromeV8ContextSet& context_set =
          extension_dispatcher()->v8_context_set();
      ChromeV8Context* context = context_set.GetCurrent();
      if (!context)
        return v8::Undefined();

      std::string extension_id = context->GetExtensionID();
      EventListenerCounts& listener_counts =
          g_listener_counts.Get()[extension_id];

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

  // MatcherID AttachFilteredEvent(string event_name, object filter)
  // event_name - Name of the event to attach.
  // filter - Which instances of the named event are we interested in.
  // returns the id assigned to the listener, which will be returned from calls
  // to MatchAgainstEventFilter where this listener matches.
  v8::Handle<v8::Value> AttachFilteredEvent(const v8::Arguments& args) {
    DCHECK_EQ(2, args.Length());
    DCHECK(args[0]->IsString());
    DCHECK(args[1]->IsObject());

    const ChromeV8ContextSet& context_set =
        extension_dispatcher()->v8_context_set();
    ChromeV8Context* context = context_set.GetCurrent();
    DCHECK(context);
    if (!context)
      return v8::Integer::New(-1);

    std::string event_name = *v8::String::AsciiValue(args[0]);
    // This method throws an exception if it returns false.
    if (!extension_dispatcher()->CheckCurrentContextAccessToExtensionAPI(
            event_name))
      return v8::Undefined();

    std::string extension_id = context->GetExtensionID();
    if (extension_id.empty())
      return v8::Integer::New(-1);

    scoped_ptr<base::DictionaryValue> filter;
    scoped_ptr<content::V8ValueConverter> converter(
        content::V8ValueConverter::create());

    base::DictionaryValue* filter_dict = NULL;
    base::Value* filter_value = converter->FromV8Value(args[1]->ToObject(),
        v8::Context::GetCurrent());
    if (!filter_value->GetAsDictionary(&filter_dict)) {
      delete filter_value;
      return v8::Integer::New(-1);
    }

    filter.reset(filter_dict);
    int id = event_filter_->AddEventMatcher(event_name, ParseEventMatcher(
        filter.get()));

    // Only send IPCs the first time a filter gets added.
    if (AddFilter(event_name, extension_id, filter.get())) {
      bool lazy = IsLazyBackgroundPage(context->extension());
      content::RenderThread::Get()->Send(
          new ExtensionHostMsg_AddFilteredListener(extension_id, event_name,
                                                   *filter, lazy));
    }

    return v8::Integer::New(id);
  }

  // Add a filter to |event_name| in |extension_id|, returning true if it
  // was the first filter for that event in that extension.
  bool AddFilter(const std::string& event_name,
                 const std::string& extension_id,
                 base::DictionaryValue* filter) {
    FilteredEventListenerCounts& counts =
        g_filtered_listener_counts.Get()[extension_id];
    FilteredEventListenerCounts::iterator it = counts.find(event_name);
    if (it == counts.end())
      counts[event_name].reset(new extensions::ValueCounter);

    int result = counts[event_name]->Add(*filter);
    return 1 == result;
  }

  // Remove a filter from |event_name| in |extension_id|, returning true if it
  // was the last filter for that event in that extension.
  bool RemoveFilter(const std::string& event_name,
                    const std::string& extension_id,
                    base::DictionaryValue* filter) {
    FilteredEventListenerCounts& counts =
        g_filtered_listener_counts.Get()[extension_id];
    FilteredEventListenerCounts::iterator it = counts.find(event_name);
    if (it == counts.end())
      return false;
    return 0 == it->second->Remove(*filter);
  }

  // void DetachFilteredEvent(int id, bool manual)
  // id     - Id of the event to detach.
  // manual - false if this is part of the extension unload process where all
  //          listeners are automatically detached.
  v8::Handle<v8::Value> DetachFilteredEvent(const v8::Arguments& args) {
    DCHECK_EQ(2, args.Length());
    DCHECK(args[0]->IsInt32());
    DCHECK(args[1]->IsBoolean());
    bool is_manual = args[1]->BooleanValue();
    const ChromeV8ContextSet& context_set =
        extension_dispatcher()->v8_context_set();
    ChromeV8Context* context = context_set.GetCurrent();
    if (!context)
      return v8::Undefined();

    std::string extension_id = context->GetExtensionID();
    if (extension_id.empty())
      return v8::Undefined();

    int matcher_id = args[0]->Int32Value();
    extensions::EventMatcher* event_matcher =
        event_filter_->GetEventMatcher(matcher_id);

    const std::string& event_name = event_filter_->GetEventName(matcher_id);

    // Only send IPCs the last time a filter gets removed.
    if (RemoveFilter(event_name, extension_id, event_matcher->value())) {
      bool lazy = is_manual && IsLazyBackgroundPage(context->extension());
      content::RenderThread::Get()->Send(
          new ExtensionHostMsg_RemoveFilteredListener(extension_id, event_name,
                                                      *event_matcher->value(),
                                                      lazy));
    }

    event_filter_->RemoveEventMatcher(matcher_id);

    return v8::Undefined();
  }

  v8::Handle<v8::Value> MatchAgainstEventFilter(const v8::Arguments& args) {
    typedef std::set<extensions::EventFilter::MatcherID> MatcherIDs;

    std::string event_name = *v8::String::AsciiValue(args[0]->ToString());
    extensions::EventFilteringInfo info = ParseFromObject(args[1]->ToObject());
    MatcherIDs matched_event_filters = event_filter_->MatchEvent(
        event_name, info);
    v8::Handle<v8::Array> array(v8::Array::New(matched_event_filters.size()));
    int i = 0;
    for (MatcherIDs::iterator it = matched_event_filters.begin();
         it != matched_event_filters.end(); ++it) {
      array->Set(v8::Integer::New(i++), v8::Integer::New(*it));
    }
    return array;
  }

  extensions::EventFilteringInfo ParseFromObject(
      v8::Handle<v8::Object> object) {
    extensions::EventFilteringInfo info;
    v8::Handle<v8::String> url(v8::String::New("url"));
    if (object->Has(url)) {
      v8::Handle<v8::Value> url_value(object->Get(url));
      info.SetURL(GURL(*v8::String::AsciiValue(url_value)));
    }
    return info;
  }

 private:
  extensions::EventFilter* event_filter_;
  bool IsLazyBackgroundPage(const Extension* extension) {
    content::RenderView* render_view = GetCurrentRenderView();
    if (!render_view)
      return false;

    ExtensionHelper* helper = ExtensionHelper::Get(render_view);
    return (extension && extension->has_lazy_background_page() &&
            helper->view_type() == chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  }

  scoped_ptr<extensions::EventMatcher> ParseEventMatcher(
      base::DictionaryValue* filter_dict) {
    return scoped_ptr<extensions::EventMatcher>(new extensions::EventMatcher(
        scoped_ptr<base::DictionaryValue>(filter_dict->DeepCopy())));
  }
};

}  // namespace

// static
ChromeV8Extension* EventBindings::Get(ExtensionDispatcher* dispatcher,
    extensions::EventFilter* event_filter) {
  return new ExtensionImpl(dispatcher, event_filter);
}
