// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/event_bindings.h"

#include <map>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "components/crx_file/id_util.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/event_filter.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/value_counter.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "url/gurl.h"

namespace extensions {

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
typedef std::map<std::string, linked_ptr<ValueCounter> >
    FilteredEventListenerCounts;

// A map of extension IDs to filtered listener counts for that extension.
base::LazyInstance<std::map<std::string, FilteredEventListenerCounts> >
    g_filtered_listener_counts = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<EventFilter> g_event_filter = LAZY_INSTANCE_INITIALIZER;

std::string GetKeyForScriptContext(ScriptContext* script_context) {
  const std::string& extension_id = script_context->GetExtensionID();
  CHECK(crx_file::id_util::IdIsValid(extension_id) ||
        script_context->GetURL().is_valid());
  return crx_file::id_util::IdIsValid(extension_id)
             ? extension_id
             : script_context->GetURL().spec();
}

// Increments the number of event-listeners for the given |event_name| and
// ScriptContext. Returns the count after the increment.
int IncrementEventListenerCount(ScriptContext* script_context,
                                const std::string& event_name) {
  return ++g_listener_counts
               .Get()[GetKeyForScriptContext(script_context)][event_name];
}

// Decrements the number of event-listeners for the given |event_name| and
// ScriptContext. Returns the count after the increment.
int DecrementEventListenerCount(ScriptContext* script_context,
                                const std::string& event_name) {
  return --g_listener_counts
               .Get()[GetKeyForScriptContext(script_context)][event_name];
}

EventFilteringInfo ParseFromObject(v8::Local<v8::Object> object,
                                   v8::Isolate* isolate) {
  EventFilteringInfo info;
  v8::Local<v8::String> url(v8::String::NewFromUtf8(isolate, "url"));
  if (object->Has(url)) {
    v8::Local<v8::Value> url_value(object->Get(url));
    info.SetURL(GURL(*v8::String::Utf8Value(url_value)));
  }
  v8::Local<v8::String> instance_id(
      v8::String::NewFromUtf8(isolate, "instanceId"));
  if (object->Has(instance_id)) {
    v8::Local<v8::Value> instance_id_value(object->Get(instance_id));
    info.SetInstanceID(instance_id_value->IntegerValue());
  }
  v8::Local<v8::String> service_type(
      v8::String::NewFromUtf8(isolate, "serviceType"));
  if (object->Has(service_type)) {
    v8::Local<v8::Value> service_type_value(object->Get(service_type));
    info.SetServiceType(*v8::String::Utf8Value(service_type_value));
  }
  return info;
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
    counts[event_name].reset(new ValueCounter);

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

}  // namespace

EventBindings::EventBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("AttachEvent", base::Bind(&EventBindings::AttachEventHandler,
                                          base::Unretained(this)));
  RouteFunction("DetachEvent", base::Bind(&EventBindings::DetachEventHandler,
                                          base::Unretained(this)));
  RouteFunction(
      "AttachFilteredEvent",
      base::Bind(&EventBindings::AttachFilteredEvent, base::Unretained(this)));
  RouteFunction(
      "DetachFilteredEvent",
      base::Bind(&EventBindings::DetachFilteredEvent, base::Unretained(this)));
  RouteFunction("MatchAgainstEventFilter",
                base::Bind(&EventBindings::MatchAgainstEventFilter,
                           base::Unretained(this)));

  // It's safe to use base::Unretained here because |context| will always
  // outlive us.
  context->AddInvalidationObserver(
      base::Bind(&EventBindings::OnInvalidated, base::Unretained(this)));
}

EventBindings::~EventBindings() {}

void EventBindings::AttachEventHandler(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsString());
  AttachEvent(*v8::String::Utf8Value(args[0]));
}

void EventBindings::AttachEvent(const std::string& event_name) {
  if (!context()->HasAccessOrThrowError(event_name))
    return;

  // Record the attachment for this context so that events can be detached when
  // the context is destroyed.
  //
  // Ideally we'd CHECK that it's not already attached, however that's not
  // possible because extensions can create and attach events themselves. Very
  // silly, but that's the way it is. For an example of this, see
  // chrome/test/data/extensions/api_test/events/background.js.
  attached_event_names_.insert(event_name);

  const std::string& extension_id = context()->GetExtensionID();
  if (IncrementEventListenerCount(context(), event_name) == 1) {
    content::RenderThread::Get()->Send(new ExtensionHostMsg_AddListener(
        extension_id, context()->GetURL(), event_name));
  }

  // This is called the first time the page has added a listener. Since
  // the background page is the only lazy page, we know this is the first
  // time this listener has been registered.
  if (ExtensionFrameHelper::IsContextForEventPage(context())) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_AddLazyListener(extension_id, event_name));
  }
}

void EventBindings::DetachEventHandler(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsBoolean());
  DetachEvent(*v8::String::Utf8Value(args[0]), args[1]->BooleanValue());
}

void EventBindings::DetachEvent(const std::string& event_name, bool is_manual) {
  // See comment in AttachEvent().
  attached_event_names_.erase(event_name);

  const std::string& extension_id = context()->GetExtensionID();

  if (DecrementEventListenerCount(context(), event_name) == 0) {
    content::RenderThread::Get()->Send(new ExtensionHostMsg_RemoveListener(
        extension_id, context()->GetURL(), event_name));
  }

  // DetachEvent is called when the last listener for the context is
  // removed. If the context is the background page, and it removes the
  // last listener manually, then we assume that it is no longer interested
  // in being awakened for this event.
  if (is_manual && ExtensionFrameHelper::IsContextForEventPage(context())) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_RemoveLazyListener(extension_id, event_name));
  }
}

// MatcherID AttachFilteredEvent(string event_name, object filter)
// event_name - Name of the event to attach.
// filter - Which instances of the named event are we interested in.
// returns the id assigned to the listener, which will be returned from calls
// to MatchAgainstEventFilter where this listener matches.
void EventBindings::AttachFilteredEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsObject());

  std::string event_name = *v8::String::Utf8Value(args[0]);
  if (!context()->HasAccessOrThrowError(event_name))
    return;

  std::string extension_id = context()->GetExtensionID();

  scoped_ptr<base::DictionaryValue> filter;
  scoped_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());

  base::DictionaryValue* filter_dict = NULL;
  base::Value* filter_value = converter->FromV8Value(
      v8::Local<v8::Object>::Cast(args[1]), context()->v8_context());
  if (!filter_value) {
    args.GetReturnValue().Set(static_cast<int32_t>(-1));
    return;
  }
  if (!filter_value->GetAsDictionary(&filter_dict)) {
    delete filter_value;
    args.GetReturnValue().Set(static_cast<int32_t>(-1));
    return;
  }

  filter.reset(filter_dict);
  EventFilter& event_filter = g_event_filter.Get();
  int id =
      event_filter.AddEventMatcher(event_name, ParseEventMatcher(filter.get()));

  // Only send IPCs the first time a filter gets added.
  if (AddFilter(event_name, extension_id, filter.get())) {
    bool lazy = ExtensionFrameHelper::IsContextForEventPage(context());
    content::RenderThread::Get()->Send(new ExtensionHostMsg_AddFilteredListener(
        extension_id, event_name, *filter, lazy));
  }

  args.GetReturnValue().Set(static_cast<int32_t>(id));
}

void EventBindings::DetachFilteredEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsBoolean());
  bool is_manual = args[1]->BooleanValue();

  std::string extension_id = context()->GetExtensionID();

  int matcher_id = args[0]->Int32Value();
  EventFilter& event_filter = g_event_filter.Get();
  EventMatcher* event_matcher = event_filter.GetEventMatcher(matcher_id);

  const std::string& event_name = event_filter.GetEventName(matcher_id);

  // Only send IPCs the last time a filter gets removed.
  if (RemoveFilter(event_name, extension_id, event_matcher->value())) {
    bool remove_lazy =
        is_manual && ExtensionFrameHelper::IsContextForEventPage(context());
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_RemoveFilteredListener(
            extension_id, event_name, *event_matcher->value(), remove_lazy));
  }

  event_filter.RemoveEventMatcher(matcher_id);
}

void EventBindings::MatchAgainstEventFilter(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  typedef std::set<EventFilter::MatcherID> MatcherIDs;
  EventFilter& event_filter = g_event_filter.Get();
  std::string event_name = *v8::String::Utf8Value(args[0]);
  EventFilteringInfo info =
      ParseFromObject(args[1]->ToObject(isolate), isolate);
  // Only match events routed to this context's RenderView or ones that don't
  // have a routingId in their filter.
  MatcherIDs matched_event_filters = event_filter.MatchEvent(
      event_name, info, context()->GetRenderView()->GetRoutingID());
  v8::Local<v8::Array> array(
      v8::Array::New(isolate, matched_event_filters.size()));
  int i = 0;
  for (MatcherIDs::iterator it = matched_event_filters.begin();
       it != matched_event_filters.end();
       ++it) {
    array->Set(v8::Integer::New(isolate, i++), v8::Integer::New(isolate, *it));
  }
  args.GetReturnValue().Set(array);
}

scoped_ptr<EventMatcher> EventBindings::ParseEventMatcher(
    base::DictionaryValue* filter_dict) {
  return scoped_ptr<EventMatcher>(new EventMatcher(
      scoped_ptr<base::DictionaryValue>(filter_dict->DeepCopy()),
      context()->GetRenderView()->GetRoutingID()));
}

void EventBindings::OnInvalidated() {
  // Detach all attached events that weren't attached. Iterate over a copy
  // because it will be mutated.
  std::set<std::string> attached_event_names_safe = attached_event_names_;
  for (const std::string& event_name : attached_event_names_safe) {
    DetachEvent(event_name, false /* is_manual */);
  }
  DCHECK(attached_event_names_.empty())
      << "Events cannot be attached during invalidation";
}

}  // namespace extensions
