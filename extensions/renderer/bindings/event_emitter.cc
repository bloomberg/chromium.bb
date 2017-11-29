// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/event_emitter.h"

#include <algorithm>

#include "extensions/renderer/bindings/api_event_listeners.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/data_object_builder.h"
#include "gin/object_template_builder.h"
#include "gin/per_context_data.h"

namespace extensions {

gin::WrapperInfo EventEmitter::kWrapperInfo = {gin::kEmbedderNativeGin};

EventEmitter::EventEmitter(bool supports_filters,
                           std::unique_ptr<APIEventListeners> listeners,
                           ExceptionHandler* exception_handler)
    : supports_filters_(supports_filters),
      listeners_(std::move(listeners)),
      exception_handler_(exception_handler) {}

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
  bool run_sync = false;
  DispatchImpl(context, args, filter, run_sync, nullptr);
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

  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();
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

  listeners_->RemoveListener(listener, arguments->GetHolderCreationContext());
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

  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  std::vector<v8::Local<v8::Value>> v8_args = arguments->GetAll();

  // Dispatch() is called from JS, and sometimes expects a return value of an
  // array with entries for each of the results of the listeners. Since this is
  // directly from JS, we know it should be safe to call synchronously and use
  // the return result, so we don't use Fire().
  // TODO(devlin): It'd be nice to refactor anything expecting a result here so
  // we don't have to have this special logic, especially since script could
  // potentially tweak the result object through prototype manipulation (which
  // also means we should never use this for security decisions).
  bool run_sync = true;
  std::vector<v8::Global<v8::Value>> listener_responses;
  DispatchImpl(context, &v8_args, nullptr, run_sync, &listener_responses);

  if (!listener_responses.size()) {
    // Return nothing if there are no responses. This is the behavior of the
    // current JS implementation.
    return;
  }

  v8::Local<v8::Object> result;
  {
    v8::TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);
    v8::Local<v8::Array> v8_responses =
        v8::Array::New(isolate, listener_responses.size());
    for (size_t i = 0; i < listener_responses.size(); ++i) {
      // TODO(devlin): With more than 2^32 - 2 listeners, this can get nasty.
      // We shouldn't reach that point, but it would be good to add enforcement.
      CHECK(v8_responses
                ->CreateDataProperty(context, i,
                                     listener_responses[i].Get(isolate))
                .ToChecked());
    }

    result = gin::DataObjectBuilder(isolate)
                 .Set("results", v8_responses.As<v8::Value>())
                 .Build();
  }
  arguments->Return(result);
}

void EventEmitter::DispatchImpl(
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* args,
    const EventFilteringInfo* filter,
    bool run_sync,
    std::vector<v8::Global<v8::Value>>* out_values) {
  // Note that |listeners_| can be modified during handling.
  std::vector<v8::Local<v8::Function>> listeners =
      listeners_->GetListeners(filter, context);

  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch try_catch(isolate);
  JSRunner* js_runner = JSRunner::Get(context);
  for (const auto& listener : listeners) {
    if (run_sync) {
      DCHECK(out_values);
      v8::Global<v8::Value> result = js_runner->RunJSFunctionSync(
          listener, context, args->size(), args->data());
      if (!result.IsEmpty() && !result.Get(isolate)->IsUndefined())
        out_values->push_back(std::move(result));
    } else {
      js_runner->RunJSFunction(listener, context, args->size(), args->data());
    }

    if (try_catch.HasCaught()) {
      exception_handler_->HandleException(context, "Error in event handler",
                                          &try_catch);
    }
  }
}

}  // namespace extensions
