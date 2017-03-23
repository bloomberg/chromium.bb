// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/event_emitter.h"

#include <algorithm>

#include "extensions/renderer/api_event_listeners.h"
#include "gin/object_template_builder.h"
#include "gin/per_context_data.h"

namespace extensions {

gin::WrapperInfo EventEmitter::kWrapperInfo = {gin::kEmbedderNativeGin};

EventEmitter::EventEmitter(bool supports_filters,
                           std::unique_ptr<APIEventListeners> listeners,
                           const binding::RunJSFunction& run_js)
    : supports_filters_(supports_filters),
      listeners_(std::move(listeners)),
      run_js_(run_js) {}

EventEmitter::~EventEmitter() {}

gin::ObjectTemplateBuilder EventEmitter::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<EventEmitter>::GetObjectTemplateBuilder(isolate)
      .SetMethod("addListener", &EventEmitter::AddListener)
      .SetMethod("removeListener", &EventEmitter::RemoveListener)
      .SetMethod("hasListener", &EventEmitter::HasListener)
      .SetMethod("hasListeners", &EventEmitter::HasListeners)
      // The following methods aren't part of the public API, but are used
      // by our custom bindings and exposed on the public event object. :(
      // TODO(devlin): Once we convert all custom bindings that use these,
      // they can be removed.
      .SetMethod("dispatch", &EventEmitter::Dispatch);
}

void EventEmitter::Fire(v8::Local<v8::Context> context,
                        std::vector<v8::Local<v8::Value>>* args,
                        const EventFilteringInfo* filter) {
  // Note that |listeners_| can be modified during handling.
  std::vector<v8::Local<v8::Function>> listeners =
      listeners_->GetListeners(filter, context);

  for (const auto& listener : listeners) {
    v8::TryCatch try_catch(context->GetIsolate());
    // SetVerbose() means the error will still get logged, which is what we
    // want. We don't let it bubble up any further to prevent it from being
    // surfaced in e.g. JS code that triggered the event.
    try_catch.SetVerbose(true);
    run_js_.Run(listener, context, args->size(), args->data());
  }
}

void EventEmitter::Invalidate(v8::Local<v8::Context> context) {
  valid_ = false;
  listeners_->Invalidate(context);
}

size_t EventEmitter::GetNumListeners() const {
  return listeners_->GetNumListeners();
}

void EventEmitter::AddListener(gin::Arguments* arguments) {
  // If script from another context maintains a reference to this object, it's
  // possible that functions can be called after this object's owning context
  // is torn down and released by blink. We don't support this behavior, but
  // we need to make sure nothing crashes, so early out of methods.
  if (!valid_)
    return;

  v8::Local<v8::Function> listener;
  // TODO(devlin): For some reason, we don't throw an error when someone calls
  // add/removeListener with no argument. We probably should. For now, keep
  // the status quo, but we should revisit this.
  if (!arguments->GetNext(&listener))
    return;

  if (!arguments->PeekNext().IsEmpty() && !supports_filters_) {
    arguments->ThrowTypeError("This event does not support filters");
    return;
  }

  v8::Local<v8::Object> filter;
  if (!arguments->PeekNext().IsEmpty() && !arguments->GetNext(&filter)) {
    arguments->ThrowTypeError("Invalid invocation");
    return;
  }

  v8::Local<v8::Object> holder;
  CHECK(arguments->GetHolder(&holder));
  CHECK(!holder.IsEmpty());
  v8::Local<v8::Context> context = holder->CreationContext();
  if (!gin::PerContextData::From(context))
    return;

  std::string error;
  if (!listeners_->AddListener(listener, filter, context, &error) &&
      !error.empty()) {
    arguments->ThrowTypeError(error);
  }
}

void EventEmitter::RemoveListener(gin::Arguments* arguments) {
  // See comment in AddListener().
  if (!valid_)
    return;

  v8::Local<v8::Function> listener;
  // See comment in AddListener().
  if (!arguments->GetNext(&listener))
    return;

  v8::Local<v8::Object> holder;
  CHECK(arguments->GetHolder(&holder));
  CHECK(!holder.IsEmpty());
  v8::Local<v8::Context> context = holder->CreationContext();
  listeners_->RemoveListener(listener, context);
}

bool EventEmitter::HasListener(v8::Local<v8::Function> listener) {
  return listeners_->HasListener(listener);
}

bool EventEmitter::HasListeners() {
  return listeners_->GetNumListeners() != 0;
}

void EventEmitter::Dispatch(gin::Arguments* arguments) {
  if (!valid_)
    return;

  if (listeners_->GetNumListeners() == 0)
    return;
  v8::HandleScope handle_scope(arguments->isolate());
  v8::Local<v8::Context> context =
      arguments->isolate()->GetCurrentContext();
  std::vector<v8::Local<v8::Value>> v8_args;
  if (arguments->Length() > 0) {
    // Converting to v8::Values should never fail.
    CHECK(arguments->GetRemaining(&v8_args));
  }
  Fire(context, &v8_args, nullptr);
}

}  // namespace extensions
