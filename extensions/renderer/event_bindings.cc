// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/event_bindings.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "components/crx_file/id_util.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/constants.h"
#include "extensions/common/event_filter.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/value_counter.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/service_worker_request_sender.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "gin/converter.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// A map of event names to the number of contexts listening to that event.
// We notify the browser about event listeners when we transition between 0
// and 1.
typedef std::map<std::string, int> EventListenerCounts;

// A map of extension IDs to listener counts for that extension.
base::LazyInstance<std::map<std::string, EventListenerCounts>>::DestructorAtExit
    g_listener_counts = LAZY_INSTANCE_INITIALIZER;

// A collection of the unmanaged events (i.e., those for which the browser is
// not notified of changes) that have listeners, by context.
base::LazyInstance<std::map<ScriptContext*, std::set<std::string>>>::Leaky
    g_unmanaged_listeners = LAZY_INSTANCE_INITIALIZER;

// A map of (extension ID, event name) pairs to the filtered listener counts
// for that pair. The map is used to keep track of which filters are in effect
// for which events.  We notify the browser about filtered event listeners when
// we transition between 0 and 1.
using FilteredEventListenerKey = std::pair<std::string, std::string>;
using FilteredEventListenerCounts =
    std::map<FilteredEventListenerKey, std::unique_ptr<ValueCounter>>;
base::LazyInstance<FilteredEventListenerCounts>::DestructorAtExit
    g_filtered_listener_counts = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<EventFilter>::DestructorAtExit g_event_filter =
    LAZY_INSTANCE_INITIALIZER;

// Gets a unique string key identifier for a ScriptContext.
// TODO(kalman): Just use pointer equality...?
std::string GetKeyForScriptContext(ScriptContext* script_context) {
  const std::string& extension_id = script_context->GetExtensionID();
  CHECK(crx_file::id_util::IdIsValid(extension_id) ||
        script_context->url().is_valid());
  return crx_file::id_util::IdIsValid(extension_id)
             ? extension_id
             : script_context->url().spec();
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

// Add a filter to |event_name| in |extension_id|, returning true if it
// was the first filter for that event in that extension.
bool AddFilter(const std::string& event_name,
               const std::string& extension_id,
               const base::DictionaryValue& filter) {
  FilteredEventListenerKey key(extension_id, event_name);
  FilteredEventListenerCounts& all_counts = g_filtered_listener_counts.Get();
  FilteredEventListenerCounts::const_iterator counts = all_counts.find(key);
  if (counts == all_counts.end()) {
    counts =
        all_counts.insert(std::make_pair(key, base::MakeUnique<ValueCounter>()))
            .first;
  }
  return counts->second->Add(filter);
}

// Remove a filter from |event_name| in |extension_id|, returning true if it
// was the last filter for that event in that extension.
bool RemoveFilter(const std::string& event_name,
                  const std::string& extension_id,
                  base::DictionaryValue* filter) {
  FilteredEventListenerKey key(extension_id, event_name);
  FilteredEventListenerCounts& all_counts = g_filtered_listener_counts.Get();
  FilteredEventListenerCounts::const_iterator counts = all_counts.find(key);
  if (counts == all_counts.end())
    return false;
  // Note: Remove() returns true if it removed the last filter equivalent to
  // |filter|. If there are more equivalent filters, or if there weren't any in
  // the first place, it returns false.
  if (counts->second->Remove(*filter)) {
    if (counts->second->is_empty())
      all_counts.erase(counts);  // Clean up if there are no more filters.
    return true;
  }
  return false;
}

// Returns a v8::Array containing the ids of the listeners that match the given
// |event_filter_dict| in the given |script_context|.
v8::Local<v8::Array> GetMatchingListeners(ScriptContext* script_context,
                                          const std::string& event_name,
                                          const EventFilteringInfo& info) {
  const EventFilter& event_filter = g_event_filter.Get();
  v8::Isolate* isolate = script_context->isolate();
  v8::Local<v8::Context> context = script_context->v8_context();

  // Only match events routed to this context's RenderFrame or ones that don't
  // have a routingId in their filter.
  std::set<EventFilter::MatcherID> matched_event_filters =
      event_filter.MatchEvent(event_name, info,
                              script_context->GetRenderFrame()->GetRoutingID());
  v8::Local<v8::Array> array(
      v8::Array::New(isolate, matched_event_filters.size()));
  int i = 0;
  for (EventFilter::MatcherID id : matched_event_filters) {
    CHECK(array->CreateDataProperty(context, i++, v8::Integer::New(isolate, id))
              .ToChecked());
  }

  return array;
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
  RouteFunction("DetachFilteredEvent",
                base::Bind(&EventBindings::DetachFilteredEventHandler,
                           base::Unretained(this)));
  RouteFunction(
      "AttachUnmanagedEvent",
      base::Bind(&EventBindings::AttachUnmanagedEvent, base::Unretained(this)));
  RouteFunction(
      "DetachUnmanagedEvent",
      base::Bind(&EventBindings::DetachUnmanagedEvent, base::Unretained(this)));

  // It's safe to use base::Unretained here because |context| will always
  // outlive us.
  context->AddInvalidationObserver(
      base::Bind(&EventBindings::OnInvalidated, base::Unretained(this)));
}

EventBindings::~EventBindings() {}

bool EventBindings::HasListener(ScriptContext* script_context,
                                const std::string& event_name) {
  const auto& unmanaged_listeners = g_unmanaged_listeners.Get();
  auto unmanaged_iter = unmanaged_listeners.find(script_context);
  if (unmanaged_iter != unmanaged_listeners.end() &&
      base::ContainsKey(unmanaged_iter->second, event_name)) {
    return true;
  }
  const auto& managed_listeners = g_listener_counts.Get();
  auto managed_iter =
      managed_listeners.find(GetKeyForScriptContext(script_context));
  if (managed_iter != managed_listeners.end()) {
    auto event_iter = managed_iter->second.find(event_name);
    if (event_iter != managed_iter->second.end() && event_iter->second > 0) {
      return true;
    }
  }

  return false;
}

// static
void EventBindings::DispatchEventInContext(
    const std::string& event_name,
    const base::ListValue* event_args,
    const EventFilteringInfo* filtering_info,
    ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Context::Scope context_scope(context->v8_context());

  v8::Local<v8::Array> listener_ids;
  if (filtering_info && !filtering_info->is_empty())
    listener_ids = GetMatchingListeners(context, event_name, *filtering_info);
  else
    listener_ids = v8::Array::New(context->isolate());

  v8::Local<v8::Value> v8_args[] = {
      gin::StringToSymbol(context->isolate(), event_name),
      content::V8ValueConverter::Create()->ToV8Value(event_args,
                                                     context->v8_context()),
      listener_ids,
  };

  context->module_system()->CallModuleMethodSafe(
      kEventBindings, "dispatchEvent", arraysize(v8_args), v8_args);
}

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

  const int worker_thread_id = content::WorkerThread::GetCurrentId();
  const std::string& extension_id = context()->GetExtensionID();
  IPC::Sender* sender = GetIPCSender();
  if (IncrementEventListenerCount(context(), event_name) == 1) {
    sender->Send(new ExtensionHostMsg_AddListener(
        extension_id, context()->url(), event_name, worker_thread_id));
  }

  // This is called the first time the page has added a listener. Since
  // the background page is the only lazy page, we know this is the first
  // time this listener has been registered.
  bool is_lazy_context =
      ExtensionFrameHelper::IsContextForEventPage(context()) ||
      context()->context_type() == Feature::SERVICE_WORKER_CONTEXT;
  if (is_lazy_context) {
    sender->Send(new ExtensionHostMsg_AddLazyListener(extension_id, event_name,
                                                      worker_thread_id));
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

  int worker_thread_id = content::WorkerThread::GetCurrentId();
  IPC::Sender* sender = GetIPCSender();
  const std::string& extension_id = context()->GetExtensionID();

  if (DecrementEventListenerCount(context(), event_name) == 0) {
    sender->Send(new ExtensionHostMsg_RemoveListener(
        extension_id, context()->url(), event_name, worker_thread_id));
  }

  // DetachEvent is called when the last listener for the context is
  // removed. If the context is the background page or service worker, and it
  // removes the last listener manually, then we assume that it is no longer
  // interested in being awakened for this event.
  if (is_manual) {
    bool is_lazy_context =
        ExtensionFrameHelper::IsContextForEventPage(context()) ||
        context()->context_type() == Feature::SERVICE_WORKER_CONTEXT;
    if (is_lazy_context) {
      sender->Send(new ExtensionHostMsg_RemoveLazyListener(
          extension_id, event_name, worker_thread_id));
    }
  }
}

// MatcherID AttachFilteredEvent(string event_name, object filter)
// event_name - Name of the event to attach.
// filter - Which instances of the named event are we interested in.
// returns the id assigned to the listener, which will be provided to calls to
// dispatchEvent().
void EventBindings::AttachFilteredEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsObject());

  std::string event_name = *v8::String::Utf8Value(args[0]);
  if (!context()->HasAccessOrThrowError(event_name))
    return;

  std::unique_ptr<base::DictionaryValue> filter;
  {
    std::unique_ptr<base::Value> filter_value =
        content::V8ValueConverter::Create()->FromV8Value(
            v8::Local<v8::Object>::Cast(args[1]), context()->v8_context());
    if (!filter_value || !filter_value->IsType(base::Value::Type::DICTIONARY)) {
      args.GetReturnValue().Set(static_cast<int32_t>(-1));
      return;
    }
    filter = base::DictionaryValue::From(std::move(filter_value));
  }

  int id = g_event_filter.Get().AddEventMatcher(
      event_name,
      base::MakeUnique<EventMatcher>(
          std::move(filter), context()->GetRenderFrame()->GetRoutingID()));
  if (id == -1) {
    args.GetReturnValue().Set(static_cast<int32_t>(-1));
    return;
  }
  attached_matcher_ids_.insert(id);

  // Only send IPCs the first time a filter gets added.
  const EventMatcher* matcher = g_event_filter.Get().GetEventMatcher(id);
  DCHECK(matcher);
  base::DictionaryValue* filter_weak = matcher->value();
  std::string extension_id = context()->GetExtensionID();
  if (AddFilter(event_name, extension_id, *filter_weak)) {
    bool lazy = ExtensionFrameHelper::IsContextForEventPage(context());
    content::RenderThread::Get()->Send(new ExtensionHostMsg_AddFilteredListener(
        extension_id, event_name, *filter_weak, lazy));
  }

  args.GetReturnValue().Set(static_cast<int32_t>(id));
}

void EventBindings::DetachFilteredEventHandler(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsBoolean());
  DetachFilteredEvent(args[0]->Int32Value(), args[1]->BooleanValue());
}

void EventBindings::DetachFilteredEvent(int matcher_id, bool is_manual) {
  EventFilter& event_filter = g_event_filter.Get();
  EventMatcher* event_matcher = event_filter.GetEventMatcher(matcher_id);

  const std::string& event_name = event_filter.GetEventName(matcher_id);

  // Only send IPCs the last time a filter gets removed.
  std::string extension_id = context()->GetExtensionID();
  if (RemoveFilter(event_name, extension_id, event_matcher->value())) {
    bool remove_lazy =
        is_manual && ExtensionFrameHelper::IsContextForEventPage(context());
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_RemoveFilteredListener(
            extension_id, event_name, *event_matcher->value(), remove_lazy));
  }

  event_filter.RemoveEventMatcher(matcher_id);
  attached_matcher_ids_.erase(matcher_id);
}

void EventBindings::AttachUnmanagedEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsString());
  std::string event_name = gin::V8ToString(args[0]);
  g_unmanaged_listeners.Get()[context()].insert(event_name);
}

void EventBindings::DetachUnmanagedEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsString());
  std::string event_name = gin::V8ToString(args[0]);
  g_unmanaged_listeners.Get()[context()].erase(event_name);
}

IPC::Sender* EventBindings::GetIPCSender() {
  const bool is_service_worker_context =
      context()->context_type() == Feature::SERVICE_WORKER_CONTEXT;
  DCHECK_EQ(is_service_worker_context,
            content::WorkerThread::GetCurrentId() != kNonWorkerThreadId);
  return is_service_worker_context
             ? static_cast<IPC::Sender*>(WorkerThreadDispatcher::Get())
             : static_cast<IPC::Sender*>(content::RenderThread::Get());
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

  // Same for filtered events.
  std::set<int> attached_matcher_ids_safe = attached_matcher_ids_;
  for (int matcher_id : attached_matcher_ids_safe) {
    DetachFilteredEvent(matcher_id, false /* is_manual */);
  }
  DCHECK(attached_matcher_ids_.empty())
      << "Filtered events cannot be attached during invalidation";

  g_unmanaged_listeners.Get().erase(context());
}

}  // namespace extensions
