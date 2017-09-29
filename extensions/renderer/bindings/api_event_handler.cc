// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_event_handler.h"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "extensions/renderer/bindings/api_event_listeners.h"
#include "extensions/renderer/bindings/event_emitter.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"

namespace extensions {

namespace {

void DoNothingOnListenersChanged(binding::EventListenersChanged change,
                                 const base::DictionaryValue* filter,
                                 bool update_lazy_listeners,
                                 v8::Local<v8::Context> context) {}

const char kExtensionAPIEventPerContextKey[] = "extension_api_events";

struct APIEventPerContextData : public base::SupportsUserData::Data {
  APIEventPerContextData(v8::Isolate* isolate) : isolate(isolate) {}
  ~APIEventPerContextData() override {
    DCHECK(emitters.empty())
        << "|emitters| should have been cleared by InvalidateContext()";
    DCHECK(massagers.empty())
        << "|massagers| should have been cleared by InvalidateContext()";
    DCHECK(anonymous_emitters.empty())
        << "|anonymous_emitters| should have been cleared by "
        << "InvalidateContext()";
  }

  // The associated v8::Isolate. Since this object is cleaned up at context
  // destruction, this should always be valid.
  v8::Isolate* isolate;

  // A map from event name -> event emitter.
  std::map<std::string, v8::Global<v8::Object>> emitters;

  // A map from event name -> argument massager.
  std::map<std::string, v8::Global<v8::Function>> massagers;

  // The collection of anonymous events.
  std::vector<v8::Global<v8::Object>> anonymous_emitters;
};

APIEventPerContextData* GetContextData(v8::Local<v8::Context> context,
                                       bool should_create) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data)
    return nullptr;
  auto* data = static_cast<APIEventPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIEventPerContextKey));

  if (!data && should_create) {
    auto api_data =
        std::make_unique<APIEventPerContextData>(context->GetIsolate());
    data = api_data.get();
    per_context_data->SetUserData(kExtensionAPIEventPerContextKey,
                                  std::move(api_data));
  }

  return data;
}

void DispatchEvent(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  if (info.Length() != 1 || !info[0]->IsArray()) {
    NOTREACHED();
    return;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  APIEventPerContextData* data = GetContextData(context, false);
  DCHECK(data);
  std::string event_name = gin::V8ToString(info.Data());
  auto iter = data->emitters.find(event_name);
  if (iter == data->emitters.end())
    return;
  v8::Global<v8::Object>& v8_emitter = iter->second;

  std::vector<v8::Local<v8::Value>> args;
  CHECK(gin::Converter<std::vector<v8::Local<v8::Value>>>::FromV8(
      isolate, info[0], &args));

  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(isolate, v8_emitter.Get(isolate),
                                        &emitter);
  CHECK(emitter);
  emitter->Fire(context, &args, nullptr);
}

}  // namespace

APIEventHandler::APIEventHandler(
    const binding::RunJSFunction& call_js,
    const binding::RunJSFunctionSync& call_js_sync,
    const EventListenersChangedMethod& listeners_changed,
    ExceptionHandler* exception_handler)
    : call_js_(call_js),
      call_js_sync_(call_js_sync),
      listeners_changed_(listeners_changed),
      exception_handler_(exception_handler) {}
APIEventHandler::~APIEventHandler() {}

v8::Local<v8::Object> APIEventHandler::CreateEventInstance(
    const std::string& event_name,
    bool supports_filters,
    bool supports_lazy_listeners,
    int max_listeners,
    bool notify_on_change,
    v8::Local<v8::Context> context) {
  // We need a context scope since gin::CreateHandle only takes the isolate
  // and infers the context from that.
  // TODO(devlin): This could be avoided if gin::CreateHandle could take a
  // context directly.
  v8::Context::Scope context_scope(context);

  APIEventPerContextData* data = GetContextData(context, true);
  DCHECK(data->emitters.find(event_name) == data->emitters.end());

  APIEventListeners::ListenersUpdated updated =
      notify_on_change ? base::Bind(listeners_changed_, event_name)
                       : base::Bind(&DoNothingOnListenersChanged);
  std::unique_ptr<APIEventListeners> listeners;
  if (supports_filters) {
    listeners = std::make_unique<FilteredEventListeners>(
        updated, event_name, max_listeners, supports_lazy_listeners,
        &event_filter_);
  } else {
    listeners = std::make_unique<UnfilteredEventListeners>(
        updated, max_listeners, supports_lazy_listeners);
  }

  gin::Handle<EventEmitter> emitter_handle = gin::CreateHandle(
      context->GetIsolate(),
      new EventEmitter(supports_filters, std::move(listeners), call_js_,
                       call_js_sync_, exception_handler_));
  CHECK(!emitter_handle.IsEmpty());
  v8::Local<v8::Value> emitter_value = emitter_handle.ToV8();
  CHECK(emitter_value->IsObject());
  v8::Local<v8::Object> emitter_object =
      v8::Local<v8::Object>::Cast(emitter_value);
  data->emitters[event_name] =
      v8::Global<v8::Object>(context->GetIsolate(), emitter_object);

  return emitter_object;
}

v8::Local<v8::Object> APIEventHandler::CreateAnonymousEventInstance(
    v8::Local<v8::Context> context) {
  v8::Context::Scope context_scope(context);
  APIEventPerContextData* data = GetContextData(context, true);
  bool supports_filters = false;
  std::unique_ptr<APIEventListeners> listeners =
      std::make_unique<UnfilteredEventListeners>(
          base::Bind(&DoNothingOnListenersChanged), binding::kNoListenerMax,
          false);
  gin::Handle<EventEmitter> emitter_handle = gin::CreateHandle(
      context->GetIsolate(),
      new EventEmitter(supports_filters, std::move(listeners), call_js_,
                       call_js_sync_, exception_handler_));
  CHECK(!emitter_handle.IsEmpty());
  v8::Local<v8::Object> emitter_object = emitter_handle.ToV8().As<v8::Object>();
  data->anonymous_emitters.push_back(
      v8::Global<v8::Object>(context->GetIsolate(), emitter_object));
  return emitter_object;
}

void APIEventHandler::InvalidateCustomEvent(v8::Local<v8::Context> context,
                                            v8::Local<v8::Object> event) {
  EventEmitter* emitter = nullptr;
  APIEventPerContextData* data = GetContextData(context, false);
  if (!data || !gin::Converter<EventEmitter*>::FromV8(context->GetIsolate(),
                                                      event, &emitter)) {
    NOTREACHED();
    return;
  }

  emitter->Invalidate(context);
  auto emitter_entry = std::find(data->anonymous_emitters.begin(),
                                 data->anonymous_emitters.end(), event);
  if (emitter_entry == data->anonymous_emitters.end()) {
    NOTREACHED();
    return;
  }

  data->anonymous_emitters.erase(emitter_entry);
}

void APIEventHandler::FireEventInContext(const std::string& event_name,
                                         v8::Local<v8::Context> context,
                                         const base::ListValue& args,
                                         const EventFilteringInfo* filter) {
  // Don't bother converting arguments if there are no listeners.
  // NOTE(devlin): This causes a double data and EventEmitter lookup, since
  // the v8 version below also checks for listeners. This should be very cheap,
  // but if we were really worried we could refactor.
  if (!HasListenerForEvent(event_name, context))
    return;

  // Note: since we only convert the arguments once, if a listener modifies an
  // object (including an array), other listeners will see that modification.
  // TODO(devlin): This is how it's always been, but should it be?
  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();

  std::vector<v8::Local<v8::Value>> v8_args;
  v8_args.reserve(args.GetSize());
  for (const auto& arg : args)
    v8_args.push_back(converter->ToV8Value(&arg, context));

  FireEventInContext(event_name, context, &v8_args, filter);
}

void APIEventHandler::FireEventInContext(
    const std::string& event_name,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments,
    const EventFilteringInfo* filter) {
  APIEventPerContextData* data = GetContextData(context, false);
  if (!data)
    return;

  auto iter = data->emitters.find(event_name);
  if (iter == data->emitters.end())
    return;
  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(
      context->GetIsolate(), iter->second.Get(context->GetIsolate()), &emitter);
  CHECK(emitter);

  auto massager_iter = data->massagers.find(event_name);
  if (massager_iter == data->massagers.end()) {
    emitter->Fire(context, arguments, filter);
  } else {
    v8::Isolate* isolate = context->GetIsolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Function> massager = massager_iter->second.Get(isolate);

    v8::Local<v8::Array> args_array =
        v8::Array::New(isolate, arguments->size());
    {
      // Massagers expect an array of v8 values. Since this is a newly-
      // constructed array and we're assigning data properties, this shouldn't
      // be able to fail or be visible by other script.
      for (size_t i = 0; i < arguments->size(); ++i) {
        v8::Maybe<bool> success = args_array->CreateDataProperty(
            context, static_cast<uint32_t>(i), arguments->at(i));
        CHECK(success.ToChecked());
      }
    }

    // Curry in the native dispatch function. Some argument massagers take
    // extra liberties and call this asynchronously, so we can't just have the
    // massager return a modified array of arguments.
    // We don't store this in a template because the Data (event name) is
    // different for each instance. Luckily, this is called during dispatching
    // an event, rather than e.g. at initialization time.
    v8::Local<v8::Function> dispatch_event = v8::Function::New(
        isolate, &DispatchEvent, gin::StringToSymbol(isolate, event_name));

    v8::Local<v8::Value> massager_args[] = {args_array, dispatch_event};
    call_js_.Run(massager, context, arraysize(massager_args), massager_args);
  }
}

void APIEventHandler::RegisterArgumentMassager(
    v8::Local<v8::Context> context,
    const std::string& event_name,
    v8::Local<v8::Function> massager) {
  APIEventPerContextData* data = GetContextData(context, true);
  DCHECK(data->massagers.find(event_name) == data->massagers.end());
  data->massagers[event_name].Reset(context->GetIsolate(), massager);
}

bool APIEventHandler::HasListenerForEvent(const std::string& event_name,
                                          v8::Local<v8::Context> context) {
  APIEventPerContextData* data = GetContextData(context, false);
  if (!data)
    return false;

  auto iter = data->emitters.find(event_name);
  if (iter == data->emitters.end())
    return false;
  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(
      context->GetIsolate(), iter->second.Get(context->GetIsolate()), &emitter);
  CHECK(emitter);
  return emitter->GetNumListeners() > 0;
}

void APIEventHandler::InvalidateContext(v8::Local<v8::Context> context) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIEventPerContextData* data = static_cast<APIEventPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIEventPerContextKey));
  if (!data)
    return;

  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);

  // This loop *shouldn't* allow any self-modification (i.e., no listeners
  // should be added or removed as a result of the iteration). If that changes,
  // we'll need to cache the listeners elsewhere before iterating.
  for (const auto& pair : data->emitters) {
    EventEmitter* emitter = nullptr;
    gin::Converter<EventEmitter*>::FromV8(isolate, pair.second.Get(isolate),
                                          &emitter);
    CHECK(emitter);
    emitter->Invalidate(context);
  }
  for (const auto& global : data->anonymous_emitters) {
    EventEmitter* emitter = nullptr;
    gin::Converter<EventEmitter*>::FromV8(isolate, global.Get(isolate),
                                          &emitter);
    CHECK(emitter);
    emitter->Invalidate(context);
  }

  data->emitters.clear();
  data->massagers.clear();
  data->anonymous_emitters.clear();

  // InvalidateContext() is called shortly (and, theoretically, synchronously)
  // before the PerContextData is deleted. We have a check that guarantees that
  // no new EventEmitters are created after the PerContextData is deleted, so
  // no new emitters should be created after this point.
}

size_t APIEventHandler::GetNumEventListenersForTesting(
    const std::string& event_name,
    v8::Local<v8::Context> context) {
  APIEventPerContextData* data = GetContextData(context, false);
  DCHECK(data);

  auto iter = data->emitters.find(event_name);
  DCHECK(iter != data->emitters.end());
  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(
      context->GetIsolate(), iter->second.Get(context->GetIsolate()), &emitter);
  CHECK(emitter);
  return emitter->GetNumListeners();
}

}  // namespace extensions
