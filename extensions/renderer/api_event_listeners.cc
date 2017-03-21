// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_event_listeners.h"

#include <algorithm>
#include <memory>

#include "base/memory/ptr_util.h"
#include "content/public/child/v8_value_converter.h"
#include "extensions/common/event_filter.h"
#include "extensions/common/event_filtering_info.h"
#include "extensions/common/event_matcher.h"
#include "gin/converter.h"

namespace extensions {

namespace {

// TODO(devlin): The EventFilter supports adding EventMatchers associated with
// an id. For now, we ignore it and add/return everything associated with this
// constant. We should rethink that.
const int kIgnoreRoutingId = 0;

// Pseudo-validates the given |filter| and converts it into a
// base::DictionaryValue. Returns true on success.
// TODO(devlin): This "validation" is pretty terrible. It matches the JS
// equivalent, but it's lousy and makes it easy for users to get it wrong.
// We should generate an argument spec for it and match it exactly.
bool ValidateFilter(v8::Local<v8::Context> context,
                    v8::Local<v8::Object> filter,
                    std::unique_ptr<base::DictionaryValue>* filter_dict,
                    std::string* error) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  if (filter.IsEmpty()) {
    *filter_dict = base::MakeUnique<base::DictionaryValue>();
    return true;
  }

  v8::Local<v8::Value> url_filter;
  if (!filter->Get(context, gin::StringToSymbol(isolate, "url"))
           .ToLocal(&url_filter)) {
    return false;
  }

  if (!url_filter->IsUndefined() && !url_filter->IsArray()) {
    *error = "filters.url should be an array.";
    return false;
  }

  v8::Local<v8::Value> service_type;
  if (!filter->Get(context, gin::StringToSymbol(isolate, "serviceType"))
           .ToLocal(&service_type)) {
    return false;
  }

  if (!service_type->IsUndefined() && !service_type->IsString()) {
    *error = "filters.serviceType should be a string.";
    return false;
  }

  std::unique_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  std::unique_ptr<base::Value> value = converter->FromV8Value(filter, context);
  if (!value || !value->is_dict()) {
    *error = "could not convert filter.";
    return false;
  }

  *filter_dict = base::DictionaryValue::From(std::move(value));
  return true;
}

}  // namespace

UnfilteredEventListeners::UnfilteredEventListeners(
    const ListenersUpdated& listeners_updated)
    : listeners_updated_(listeners_updated) {}
UnfilteredEventListeners::~UnfilteredEventListeners() = default;

bool UnfilteredEventListeners::AddListener(v8::Local<v8::Function> listener,
                                           v8::Local<v8::Object> filter,
                                           v8::Local<v8::Context> context,
                                           std::string* error) {
  // |filter| should be checked before getting here.
  DCHECK(filter.IsEmpty())
      << "Filtered events should use FilteredEventListeners";

  if (HasListener(listener))
    return false;

  listeners_.push_back(
      v8::Global<v8::Function>(context->GetIsolate(), listener));
  if (listeners_.size() == 1) {
    listeners_updated_.Run(binding::EventListenersChanged::HAS_LISTENERS,
                           nullptr, context);
  }

  return true;
}

void UnfilteredEventListeners::RemoveListener(v8::Local<v8::Function> listener,
                                              v8::Local<v8::Context> context) {
  auto iter = std::find(listeners_.begin(), listeners_.end(), listener);
  if (iter == listeners_.end())
    return;

  listeners_.erase(iter);
  if (listeners_.empty()) {
    listeners_updated_.Run(binding::EventListenersChanged::NO_LISTENERS,
                           nullptr, context);
  }
}

bool UnfilteredEventListeners::HasListener(v8::Local<v8::Function> listener) {
  return std::find(listeners_.begin(), listeners_.end(), listener) !=
         listeners_.end();
}

size_t UnfilteredEventListeners::GetNumListeners() {
  return listeners_.size();
}

std::vector<v8::Local<v8::Function>> UnfilteredEventListeners::GetListeners(
    const EventFilteringInfo* filter,
    v8::Local<v8::Context> context) {
  std::vector<v8::Local<v8::Function>> listeners;
  listeners.reserve(listeners_.size());
  for (const auto& listener : listeners_)
    listeners.push_back(listener.Get(context->GetIsolate()));
  return listeners;
}

void UnfilteredEventListeners::Invalidate(v8::Local<v8::Context> context) {
  if (!listeners_.empty()) {
    listeners_.clear();
    listeners_updated_.Run(binding::EventListenersChanged::NO_LISTENERS,
                           nullptr, context);
  }
}

struct FilteredEventListeners::ListenerData {
  bool operator==(v8::Local<v8::Function> other_function) const {
    // Note that we only consider the listener function here, and not the
    // filter. This implies that it's invalid to try and add the same
    // function for multiple filters.
    // TODO(devlin): It's always been this way, but should it be?
    return function == other_function;
  }

  v8::Global<v8::Function> function;
  int filter_id;
};

FilteredEventListeners::FilteredEventListeners(
    const ListenersUpdated& listeners_updated,
    const std::string& event_name,
    EventFilter* event_filter)
    : listeners_updated_(listeners_updated),
      event_name_(event_name),
      event_filter_(event_filter) {}
FilteredEventListeners::~FilteredEventListeners() = default;

bool FilteredEventListeners::AddListener(v8::Local<v8::Function> listener,
                                         v8::Local<v8::Object> filter,
                                         v8::Local<v8::Context> context,
                                         std::string* error) {
  if (HasListener(listener))
    return false;

  std::unique_ptr<base::DictionaryValue> filter_dict;
  if (!ValidateFilter(context, filter, &filter_dict, error))
    return false;

  int filter_id = event_filter_->AddEventMatcher(
      event_name_,
      base::MakeUnique<EventMatcher>(std::move(filter_dict), kIgnoreRoutingId));

  if (filter_id == -1) {
    *error = "Could not add listener";
    return false;
  }

  const EventMatcher* matcher = event_filter_->GetEventMatcher(filter_id);
  DCHECK(matcher);
  listeners_.push_back(
      {v8::Global<v8::Function>(context->GetIsolate(), listener), filter_id});
  if (value_counter_.Add(*matcher->value())) {
    listeners_updated_.Run(binding::EventListenersChanged::HAS_LISTENERS,
                           matcher->value(), context);
  }

  return true;
}

void FilteredEventListeners::RemoveListener(v8::Local<v8::Function> listener,
                                            v8::Local<v8::Context> context) {
  auto iter = std::find(listeners_.begin(), listeners_.end(), listener);
  if (iter == listeners_.end())
    return;

  ListenerData data = std::move(*iter);
  listeners_.erase(iter);

  InvalidateListener(data, context);
}

bool FilteredEventListeners::HasListener(v8::Local<v8::Function> listener) {
  return std::find(listeners_.begin(), listeners_.end(), listener) !=
         listeners_.end();
}

size_t FilteredEventListeners::GetNumListeners() {
  return listeners_.size();
}

std::vector<v8::Local<v8::Function>> FilteredEventListeners::GetListeners(
    const EventFilteringInfo* filter,
    v8::Local<v8::Context> context) {
  std::set<int> ids = event_filter_->MatchEvent(
      event_name_, filter ? *filter : EventFilteringInfo(), kIgnoreRoutingId);

  std::vector<v8::Local<v8::Function>> listeners;
  listeners.reserve(ids.size());
  for (const auto& listener : listeners_) {
    if (ids.count(listener.filter_id))
      listeners.push_back(listener.function.Get(context->GetIsolate()));
  }
  return listeners;
}

void FilteredEventListeners::Invalidate(v8::Local<v8::Context> context) {
  for (const auto& listener : listeners_)
    InvalidateListener(listener, context);
  listeners_.clear();
}

void FilteredEventListeners::InvalidateListener(
    const ListenerData& listener,
    v8::Local<v8::Context> context) {
  EventMatcher* matcher = event_filter_->GetEventMatcher(listener.filter_id);
  DCHECK(matcher);
  if (value_counter_.Remove(*matcher->value())) {
    listeners_updated_.Run(binding::EventListenersChanged::NO_LISTENERS,
                           matcher->value(), context);
  }

  event_filter_->RemoveEventMatcher(listener.filter_id);
}

}  // namespace extensions
