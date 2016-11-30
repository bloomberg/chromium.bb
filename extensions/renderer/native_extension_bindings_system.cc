// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/native_extension_bindings_system.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "gin/converter.h"
#include "gin/per_context_data.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

namespace {

const char kBindingsSystemPerContextKey[] = "extension_bindings_system";

struct BindingsSystemPerContextData : public base::SupportsUserData::Data {
  BindingsSystemPerContextData(
      base::WeakPtr<NativeExtensionBindingsSystem> bindings_system)
      : bindings_system(bindings_system) {}
  ~BindingsSystemPerContextData() override {}

  v8::Global<v8::Object> api_object;
  base::WeakPtr<NativeExtensionBindingsSystem> bindings_system;
};

// If a 'chrome' property exists on the context's global and is an object,
// returns that.
// If a 'chrome' property exists but isn't an object, returns an empty Local.
// If no 'chrome' property exists (or is undefined), creates a new
// object, assigns it to Global().chrome, and returns it.
v8::Local<v8::Object> GetOrCreateChrome(v8::Local<v8::Context> context) {
  // TODO(devlin): This is a little silly. We expect that this may do the wrong
  // thing if the window has set some other 'chrome' (as in the case of script
  // doing 'window.chrome = true'), but we don't really handle it. It could also
  // throw exceptions or have unintended side effects.
  // On the one hand, anyone writing that code is probably asking for trouble.
  // On the other, it'd be nice to avoid. I wonder if we can?
  v8::Local<v8::String> chrome_string =
      gin::StringToSymbol(context->GetIsolate(), "chrome");
  v8::Local<v8::Value> chrome_value;
  if (!context->Global()->Get(context, chrome_string).ToLocal(&chrome_value))
    return v8::Local<v8::Object>();

  if (chrome_value->IsUndefined()) {
    v8::Local<v8::Object> chrome = v8::Object::New(context->GetIsolate());
    v8::Maybe<bool> success =
        context->Global()->CreateDataProperty(context, chrome_string, chrome);
    if (!success.IsJust() || !success.FromJust())
      return v8::Local<v8::Object>();
    return chrome;
  }

  if (chrome_value->IsObject())
    return chrome_value.As<v8::Object>();

  return v8::Local<v8::Object>();
}

// Handler for calling safely into JS.
void CallJsFunction(v8::Local<v8::Function> function,
                    v8::Local<v8::Context> context,
                    int argc,
                    v8::Local<v8::Value> argv[]) {
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  CHECK(script_context);
  script_context->SafeCallFunction(function, argc, argv);
}

// Returns the API schema indicated by |api_name|.
const base::DictionaryValue& GetAPISchema(const std::string& api_name) {
  const base::DictionaryValue* schema =
      ExtensionAPI::GetSharedInstance()->GetSchema(api_name);
  CHECK(schema);
  return *schema;
}

// Returns true if the method specified by |method_name| is available to the
// given |context|.
bool IsAPIMethodAvailable(ScriptContext* context,
                          const std::string& method_name) {
  return context->GetAvailability(method_name).is_available();
}

}  // namespace

NativeExtensionBindingsSystem::NativeExtensionBindingsSystem(
    const SendIPCMethod& send_ipc)
    : send_ipc_(send_ipc),
      api_system_(base::Bind(&CallJsFunction),
                  base::Bind(&GetAPISchema),
                  base::Bind(&NativeExtensionBindingsSystem::SendRequest,
                             base::Unretained(this))),
      weak_factory_(this) {}

NativeExtensionBindingsSystem::~NativeExtensionBindingsSystem() {}

void NativeExtensionBindingsSystem::DidCreateScriptContext(
    ScriptContext* context) {}

void NativeExtensionBindingsSystem::WillReleaseScriptContext(
    ScriptContext* context) {}

void NativeExtensionBindingsSystem::UpdateBindingsForContext(
    ScriptContext* context) {
  v8::Local<v8::Context> v8_context = context->v8_context();
  v8::Local<v8::Object> chrome = GetOrCreateChrome(v8_context);
  if (chrome.IsEmpty())
    return;

  gin::PerContextData* per_context_data = gin::PerContextData::From(v8_context);
  DCHECK(per_context_data);
  BindingsSystemPerContextData* data =
      static_cast<BindingsSystemPerContextData*>(
          per_context_data->GetUserData(kBindingsSystemPerContextKey));
  if (!data) {
    auto api_data = base::MakeUnique<BindingsSystemPerContextData>(
        weak_factory_.GetWeakPtr());
    data = api_data.get();
    per_context_data->SetUserData(kBindingsSystemPerContextKey,
                                  api_data.release());
  }

  const FeatureProvider* api_feature_provider =
      FeatureProvider::GetAPIFeatures();
  for (const auto& map_entry : api_feature_provider->GetAllFeatures()) {
    // Internal APIs are included via require(api_name) from internal code
    // rather than chrome[api_name].
    if (map_entry.second->IsInternal())
      continue;

    // If this API has a parent feature (and isn't marked 'noparent'),
    // then this must be a function or event, so we should not register.
    if (api_feature_provider->GetParent(map_entry.second.get()) != nullptr)
      continue;

    // Skip chrome.test if this isn't a test.
    if (map_entry.first == "test" &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kTestType)) {
      continue;
    }

    // TODO(devlin): UpdateBindingsForContext can be called during context
    // creation, but also when e.g. permissions change. We need to be checking
    // for whether or not the API already exists on the object as well as
    // if we need to remove any existing APIs.
    if (!context->IsAnyFeatureAvailableToContext(*map_entry.second,
                                                 CheckAliasStatus::NOT_ALLOWED))
      continue;

    v8::Local<v8::String> api_name =
        gin::StringToSymbol(v8_context->GetIsolate(), map_entry.first);
    v8::Maybe<bool> success = chrome->SetAccessor(
        v8_context, api_name, &GetAPIHelper, nullptr, api_name);
    if (!success.IsJust() || !success.FromJust()) {
      LOG(ERROR) << "Failed to create API on Chrome object.";
      return;
    }
  }
}

void NativeExtensionBindingsSystem::DispatchEventInContext(
    const std::string& event_name,
    const base::ListValue* event_args,
    const base::DictionaryValue* filtering_info,
    ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Context::Scope context_scope(context->v8_context());
  // TODO(devlin): Take into account |filtering_info|.
  api_system_.FireEventInContext(event_name, context->v8_context(),
                                 *event_args);
}

void NativeExtensionBindingsSystem::HandleResponse(
    int request_id,
    bool success,
    const base::ListValue& response,
    const std::string& error) {
  api_system_.CompleteRequest(request_id, response);
}

RequestSender* NativeExtensionBindingsSystem::GetRequestSender() {
  return nullptr;
}

// static
void NativeExtensionBindingsSystem::GetAPIHelper(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = info.Holder()->CreationContext();
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data)
    return;  // Context is shutting down.
  BindingsSystemPerContextData* data =
      static_cast<BindingsSystemPerContextData*>(
          per_context_data->GetUserData(kBindingsSystemPerContextKey));
  CHECK(data);
  if (!data->bindings_system) {
    NOTREACHED() << "Context outlived bindings system.";
    return;
  }

  v8::Local<v8::Object> apis;
  if (data->api_object.IsEmpty()) {
    apis = v8::Object::New(isolate);
    data->api_object = v8::Global<v8::Object>(isolate, apis);
  } else {
    apis = data->api_object.Get(isolate);
  }

  // We use info.Data() to store a real name here instead of using the provided
  // one to handle any weirdness from the caller (non-existent strings, etc).
  v8::Local<v8::String> api_name = info.Data().As<v8::String>();
  v8::Local<v8::Value> result;
  v8::Maybe<bool> has_property = apis->HasRealNamedProperty(context, api_name);
  if (!has_property.IsJust())
    return;

  if (has_property.FromJust()) {
    result = apis->GetRealNamedProperty(context, api_name).ToLocalChecked();
  } else {
    std::string api_name_string;
    CHECK(gin::Converter<std::string>::FromV8(isolate, api_name,
                                              &api_name_string));
    result = data->bindings_system->api_system_.CreateAPIInstance(
        api_name_string, context, isolate,
        base::Bind(&IsAPIMethodAvailable,
                   ScriptContextSet::GetContextByV8Context(context)));
    v8::Maybe<bool> success =
        apis->CreateDataProperty(context, api_name, result);
    if (!success.IsJust() || !success.FromJust())
      return;
  }
  info.GetReturnValue().Set(result);
}

void NativeExtensionBindingsSystem::SendRequest(
    std::unique_ptr<APIBindingsSystem::Request> request,
    v8::Local<v8::Context> context) {
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);

  GURL url;
  blink::WebLocalFrame* frame = script_context->web_frame();
  if (frame && !frame->document().isNull())
    url = frame->document().url();
  else
    url = script_context->url();

  ExtensionHostMsg_Request_Params params;
  params.name = request->method_name;
  params.arguments.Swap(request->arguments.get());
  params.extension_id = script_context->GetExtensionID();
  params.source_url = url;
  params.request_id = request->request_id;
  params.has_callback = request->has_callback;
  params.user_gesture = request->has_user_gesture;
  // TODO(devlin): Make this work in ServiceWorkers.
  params.worker_thread_id = -1;
  params.service_worker_version_id = kInvalidServiceWorkerVersionId;

  send_ipc_.Run(script_context, params);
}

}  // namespace extensions
