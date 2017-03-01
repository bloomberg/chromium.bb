// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/chrome_setting.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/api_event_handler.h"
#include "extensions/renderer/api_request_handler.h"
#include "extensions/renderer/api_signature.h"
#include "extensions/renderer/api_type_reference_map.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"

namespace extensions {

v8::Local<v8::Object> ChromeSetting::Create(v8::Local<v8::Context> context,
                                            const std::string& property_name,
                                            APIRequestHandler* request_handler,
                                            APIEventHandler* event_handler,
                                            APITypeReferenceMap* type_refs) {
  auto value_spec = base::MakeUnique<base::DictionaryValue>();
  // Most ChromeSettings have a pref that matches the property name and are of
  // type boolean.
  base::StringPiece pref_name = property_name;
  base::StringPiece type = "boolean";

  // A few of ChromeSettings are special, and have different arguments.
  base::StringPiece ref;
  if (property_name == "animationPolicy") {
    type = "string";
    auto enum_list = base::MakeUnique<base::ListValue>();
    enum_list->AppendString("allowed");
    enum_list->AppendString("once");
    enum_list->AppendString("none");
    value_spec->Set("enum", std::move(enum_list));
  } else if (property_name == "webRTCIPHandlingPolicy") {
    ref = "IPHandlingPolicy";
  } else if (property_name == "spdyProxyEnabled") {
    pref_name = "spdy_proxy.enabled";
  } else if (property_name == "dataReductionDailyContentLength") {
    type = "array";
    pref_name = "data_reduction.daily_original_length";
  } else if (property_name == "dataReductionDailyReceivedLength") {
    type = "array";
    pref_name = "data_reduction.daily_received_length";
  } else if (property_name == "dataUsageReportingEnabled") {
    pref_name = "data_usage_reporting.enabled";
  } else if (property_name == "proxy") {
    ref = "ProxyConfig";
  }

  if (!ref.empty())
    value_spec->SetString("$ref", ref);
  else
    value_spec->SetString("type", type);

  // The set() call takes an object { value: { type: <t> }, ... }, where <t>
  // is the custom set() argument specified above by value_spec.
  base::DictionaryValue set_spec;
  set_spec.SetString("type", "object");
  auto properties = base::MakeUnique<base::DictionaryValue>();
  properties->Set("value", std::move(value_spec));
  set_spec.Set("properties", std::move(properties));

  gin::Handle<ChromeSetting> handle = gin::CreateHandle(
      context->GetIsolate(),
      new ChromeSetting(request_handler, event_handler, type_refs,
                        pref_name.as_string(), set_spec));
  return handle.ToV8().As<v8::Object>();
}

ChromeSetting::ChromeSetting(APIRequestHandler* request_handler,
                             APIEventHandler* event_handler,
                             const APITypeReferenceMap* type_refs,
                             const std::string& pref_name,
                             const base::DictionaryValue& argument_spec)
    : request_handler_(request_handler),
      event_handler_(event_handler),
      type_refs_(type_refs),
      pref_name_(pref_name),
      argument_spec_(argument_spec) {}

ChromeSetting::~ChromeSetting() = default;

gin::WrapperInfo ChromeSetting::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder ChromeSetting::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<ChromeSetting>::GetObjectTemplateBuilder(isolate)
      .SetMethod("get", &ChromeSetting::Get)
      .SetMethod("set", &ChromeSetting::Set)
      .SetMethod("clear", &ChromeSetting::Clear)
      .SetProperty("onChange", &ChromeSetting::GetOnChangeEvent);
}

void ChromeSetting::Get(gin::Arguments* arguments) {
  HandleFunction("get", arguments);
}

void ChromeSetting::Set(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Object> holder;
  CHECK(arguments->GetHolder(&holder));
  v8::Local<v8::Context> context = holder->CreationContext();

  v8::Local<v8::Value> value = arguments->PeekNext();
  // The set schema included in the Schema object is generic, since it varies
  // per-setting. However, this is only ever for a single setting, so we can
  // enforce the types more thoroughly.
  std::string error;
  if (!value.IsEmpty() && !argument_spec_.ParseArgument(
                              context, value, *type_refs_, nullptr, &error)) {
    arguments->ThrowTypeError("Invalid invocation");
    return;
  }
  HandleFunction("set", arguments);
}

void ChromeSetting::Clear(gin::Arguments* arguments) {
  HandleFunction("clear", arguments);
}

v8::Local<v8::Value> ChromeSetting::GetOnChangeEvent(
    gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::Local<v8::Object> holder;
  CHECK(arguments->GetHolder(&holder));
  v8::Local<v8::Context> context = holder->CreationContext();
  v8::Local<v8::Object> wrapper = GetWrapper(isolate);
  v8::Local<v8::Private> key = v8::Private::ForApi(
      isolate, gin::StringToSymbol(isolate, "onChangeEvent"));
  v8::Local<v8::Value> event;
  if (!wrapper->GetPrivate(context, key).ToLocal(&event)) {
    NOTREACHED();
    return v8::Local<v8::Value>();
  }

  DCHECK(!event.IsEmpty());
  if (event->IsUndefined()) {
    event = event_handler_->CreateEventInstance(
        base::StringPrintf("types.ChromeSetting.%s.onChange",
                           pref_name_.c_str()),
        context);
    v8::Maybe<bool> set_result = wrapper->SetPrivate(context, key, event);
    if (!set_result.IsJust() || !set_result.FromJust()) {
      NOTREACHED();
      return v8::Local<v8::Value>();
    }
  }
  return event;
}

void ChromeSetting::HandleFunction(const std::string& method_name,
                                   gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> holder;
  CHECK(arguments->GetHolder(&holder));
  v8::Local<v8::Context> context = holder->CreationContext();

  std::vector<v8::Local<v8::Value>> argument_list;
  if (arguments->Length() > 0) {
    // Just copying handles should never fail.
    CHECK(arguments->GetRemaining(&argument_list));
  }

  std::string full_name = "types.ChromeSetting." + method_name;
  std::unique_ptr<base::ListValue> converted_arguments;
  v8::Local<v8::Function> callback;
  std::string error;
  if (!type_refs_->GetTypeMethodSignature(full_name)->ParseArgumentsToJSON(
          context, argument_list, *type_refs_, &converted_arguments, &callback,
          &error)) {
    arguments->ThrowTypeError("Invalid invocation");
    return;
  }

  converted_arguments->Insert(0u, base::MakeUnique<base::Value>(pref_name_));
  request_handler_->StartRequest(context, full_name,
                                 std::move(converted_arguments), callback,
                                 v8::Local<v8::Function>());
}

}  // namespace extensions
