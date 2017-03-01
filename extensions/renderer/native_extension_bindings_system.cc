// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/native_extension_bindings_system.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/renderer/api_binding_bridge.h"
#include "extensions/renderer/api_binding_hooks.h"
#include "extensions/renderer/chrome_setting.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/storage_area.h"
#include "gin/converter.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

namespace {

const char kBindingsSystemPerContextKey[] = "extension_bindings_system";

// Returns true if the given |api| is a "prefixed" api of the |root_api|; that
// is, if the api begins with the root.
// For example, 'app.runtime' is a prefixed api of 'app'.
// This is designed to be used as a utility when iterating over a sorted map, so
// assumes that |api| is lexicographically greater than |root_api|.
bool IsPrefixedAPI(base::StringPiece api, base::StringPiece root_api) {
  DCHECK_NE(api, root_api);
  DCHECK_GT(api, root_api);
  return base::StartsWith(api, root_api, base::CompareCase::SENSITIVE) &&
         api[root_api.size()] == '.';
}

// Returns the first different level of the api specification between the given
// |api_name| and |reference|. For an api_name of 'app.runtime' and a reference
// of 'app', this returns 'app.runtime'. For an api_name of
// 'cast.streaming.session' and a reference of 'cast', this returns
// 'cast.streaming'. If reference is empty, this simply returns the first layer;
// so given 'app.runtime' and no reference, this returns 'app'.
base::StringPiece GetFirstDifferentAPIName(
    base::StringPiece api_name,
    base::StringPiece reference) {
  base::StringPiece::size_type dot =
      api_name.find('.', reference.empty() ? 0 : reference.size() + 1);
  if (dot == base::StringPiece::npos)
    return api_name;
  return api_name.substr(0, dot);
}

struct BindingsSystemPerContextData : public base::SupportsUserData::Data {
  BindingsSystemPerContextData(
      base::WeakPtr<NativeExtensionBindingsSystem> bindings_system)
      : bindings_system(bindings_system) {}
  ~BindingsSystemPerContextData() override {}

  v8::Global<v8::Object> api_object;
  v8::Global<v8::Object> internal_apis;
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

BindingsSystemPerContextData* GetBindingsDataFromContext(
    v8::Local<v8::Context> context) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data)
    return nullptr;  // Context is shutting down.

  auto* data = static_cast<BindingsSystemPerContextData*>(
      per_context_data->GetUserData(kBindingsSystemPerContextKey));
  CHECK(data);
  if (!data->bindings_system) {
    NOTREACHED() << "Context outlived bindings system.";
    return nullptr;
  }

  return data;
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

v8::Global<v8::Value> CallJsFunctionSync(v8::Local<v8::Function> function,
                                         v8::Local<v8::Context> context,
                                         int argc,
                                         v8::Local<v8::Value> argv[]) {
  bool did_complete = false;
  v8::Global<v8::Value> result;
  auto callback = base::Bind([](
      v8::Isolate* isolate,
      bool* did_complete_out,
      v8::Global<v8::Value>* result_out,
      const std::vector<v8::Local<v8::Value>>& results) {
    *did_complete_out = true;
    // The locals are released after the callback is executed, so we need to
    // grab a persistent handle.
    if (!results.empty() && !results[0].IsEmpty())
      result_out->Reset(isolate, results[0]);
  }, base::Unretained(context->GetIsolate()),
     base::Unretained(&did_complete), base::Unretained(&result));

  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  CHECK(script_context);
  script_context->SafeCallFunction(function, argc, argv, callback);
  CHECK(did_complete) << "expected script to execute synchronously";
  return result;
}

// Returns the API schema indicated by |api_name|.
const base::DictionaryValue& GetAPISchema(const std::string& api_name) {
  const base::DictionaryValue* schema =
      ExtensionAPI::GetSharedInstance()->GetSchema(api_name);
  CHECK(schema) << api_name;
  return *schema;
}

// Returns true if the method specified by |method_name| is available to the
// given |context|.
bool IsAPIMethodAvailable(ScriptContext* context,
                          const std::string& method_name) {
  return context->GetAvailability(method_name).is_available();
}

// Instantiates the binding object for the given |name|. |name| must specify a
// specific feature.
v8::Local<v8::Object> CreateRootBinding(v8::Local<v8::Context> context,
                                        ScriptContext* script_context,
                                        const std::string& name,
                                        APIBindingsSystem* bindings_system) {
  APIBindingHooks* hooks = nullptr;
  v8::Local<v8::Object> binding_object = bindings_system->CreateAPIInstance(
      name, context, context->GetIsolate(),
      base::Bind(&IsAPIMethodAvailable, script_context), &hooks);

  gin::Handle<APIBindingBridge> bridge_handle = gin::CreateHandle(
      context->GetIsolate(),
      new APIBindingBridge(bindings_system->type_reference_map(),
                           bindings_system->request_handler(), hooks, context,
                           binding_object, script_context->GetExtensionID(),
                           script_context->GetContextTypeDescription(),
                           base::Bind(&CallJsFunction)));
  v8::Local<v8::Value> native_api_bridge = bridge_handle.ToV8();
  script_context->module_system()->OnNativeBindingCreated(name,
                                                          native_api_bridge);

  return binding_object;
}

// Creates the binding object for the given |root_name|. This can be
// complicated, since APIs may have prefixed names, like 'app.runtime' or
// 'system.cpu'. This method accepts the first name (i.e., the key that we are
// looking for on the chrome object, such as 'app') and returns the fully
// instantiated binding, including prefixed APIs. That is, given 'app', this
// will instantiate 'app', 'app.runtime', and 'app.window'.
//
// NOTE(devlin): We could do the prefixed apis lazily; however, it's not clear
// how much of a win it would be. It's less overhead here than in the general
// case (instantiating a handful of APIs instead of all of them), and it's more
// likely they will be used (since the extension is already accessing the
// parent).
// TODO(devlin): We should be creating ObjectTemplates for these so that we only
// do this work once. APIBindings (for the single API) already do this.
v8::Local<v8::Object> CreateFullBinding(
    v8::Local<v8::Context> context,
    ScriptContext* script_context,
    APIBindingsSystem* bindings_system,
    const FeatureProvider* api_feature_provider,
    const std::string& root_name) {
  const FeatureMap& features = api_feature_provider->GetAllFeatures();
  auto lower = features.lower_bound(root_name);
  DCHECK(lower != features.end());

  // Some bindings have a prefixed name, like app.runtime, where 'app' and
  // 'app.runtime' are, in fact, separate APIs. It's also possible for a
  // context to have access to 'app.runtime', but not to 'app'. For this, we
  // either instantiate the 'app' binding fully (if the context has access), or
  // else use an empty object (so we can still instantiate 'app.runtime').
  v8::Local<v8::Object> root_binding;
  if (lower->first == root_name) {
    if (script_context->IsAnyFeatureAvailableToContext(
            *lower->second, CheckAliasStatus::NOT_ALLOWED)) {
      root_binding = CreateRootBinding(context, script_context, root_name,
                                       bindings_system);
    }
    ++lower;
  }

  // Look for any bindings that would be on the same object. Any of these would
  // start with the same base name (e.g. 'app') + '.' (since '.' is < x for any
  // isalpha(x)).
  std::string upper = root_name + static_cast<char>('.' + 1);
  base::StringPiece last_binding_name;
  // The following loop is a little painful because we have crazy binding names
  // and syntaxes. The way this works is as follows:
  // Look at each feature after the root feature we passed in. If there exists
  // a (non-child) feature with a prefixed name, create the full binding for
  // the object that the next feature is on. Then, iterate past any features
  // already instantiated by that, and continue until there are no more features
  // prefixed by the root API.
  // As a concrete example, we can look at the cast APIs (cast and
  // cast.streaming.*)
  // Start with vanilla 'cast', and instantiate that.
  // Then iterate over features, and see 'cast.streaming.receiverSession'.
  // 'cast.streaming.receiverSession' is a prefixed API of 'cast', but we find
  // the first level of difference, which is 'cast.streaming', and instantiate
  // that object completely (through recursion).
  // The next feature is 'cast.streaming.rtpStream', but this is a prefixed API
  // of 'cast.streaming', which we just instantiated completely (including
  // 'cast.streaming.rtpStream'), so we continue.
  // Iterate until all cast.* features are created.
  // TODO(devlin): This is bonkers, but what's the better way? We could extract
  // this out to be a more readable Visitor implementation, but is it worth it
  // for this one place? Ideally, we'd have a less convoluted feature
  // representation (some kind of tree would make this trivial), but for now, we
  // have strings.
  // On the upside, most APIs are not prefixed at all, and this loop is never
  // entered.
  for (auto iter = lower; iter != features.end() && iter->first < upper;
       ++iter) {
    if (iter->second->IsInternal())
      continue;

    if (IsPrefixedAPI(iter->first, last_binding_name)) {
      // Instantiating |last_binding_name| must have already instantiated
      // iter->first.
      continue;
    }

    // If this API has a parent feature (and isn't marked 'noparent'),
    // then this must be a function or event, so we should not register.
    if (api_feature_provider->GetParent(iter->second.get()) != nullptr)
      continue;

    base::StringPiece binding_name =
        GetFirstDifferentAPIName(iter->first, root_name);

    v8::Local<v8::Object> nested_binding =
        CreateFullBinding(context, script_context, bindings_system,
                          api_feature_provider, binding_name.as_string());
    // It's possible that we don't create a binding if no features or
    // prefixed features are available to the context.
    if (nested_binding.IsEmpty())
      continue;

    if (root_binding.IsEmpty())
      root_binding = v8::Object::New(context->GetIsolate());

    // The nested api name contains a '.', e.g. 'app.runtime', but we want to
    // expose it on the object simply as 'runtime'.
    // Cache the last_binding_name now before mangling it.
    last_binding_name = binding_name;
    DCHECK_NE(base::StringPiece::npos, binding_name.rfind('.'));
    base::StringPiece accessor_name =
        binding_name.substr(binding_name.rfind('.') + 1);
    v8::Local<v8::String> nested_name =
        gin::StringToSymbol(context->GetIsolate(), accessor_name);
    v8::Maybe<bool> success =
        root_binding->CreateDataProperty(context, nested_name, nested_binding);
    if (!success.IsJust() || !success.FromJust())
      return v8::Local<v8::Object>();
  }

  return root_binding;
}

}  // namespace

NativeExtensionBindingsSystem::NativeExtensionBindingsSystem(
    const SendRequestIPCMethod& send_request_ipc,
    const SendEventListenerIPCMethod& send_event_listener_ipc)
    : send_request_ipc_(send_request_ipc),
      send_event_listener_ipc_(send_event_listener_ipc),
      api_system_(
          base::Bind(&CallJsFunction),
          base::Bind(&CallJsFunctionSync),
          base::Bind(&GetAPISchema),
          base::Bind(&NativeExtensionBindingsSystem::SendRequest,
                     base::Unretained(this)),
          base::Bind(&NativeExtensionBindingsSystem::OnEventListenerChanged,
                     base::Unretained(this)),
          APILastError(base::Bind(&GetRuntime))),
      weak_factory_(this) {
  api_system_.RegisterCustomType("storage.StorageArea",
                                 base::Bind(&StorageArea::CreateStorageArea));
  api_system_.RegisterCustomType("types.ChromeSetting",
                                 base::Bind(&ChromeSetting::Create));
}

NativeExtensionBindingsSystem::~NativeExtensionBindingsSystem() {}

void NativeExtensionBindingsSystem::DidCreateScriptContext(
    ScriptContext* context) {
  v8::Isolate* isolate = context->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = context->v8_context();
  gin::PerContextData* per_context_data = gin::PerContextData::From(v8_context);
  DCHECK(per_context_data);
  DCHECK(!per_context_data->GetUserData(kBindingsSystemPerContextKey));
  auto data = base::MakeUnique<BindingsSystemPerContextData>(
      weak_factory_.GetWeakPtr());
  per_context_data->SetUserData(kBindingsSystemPerContextKey, data.release());

  if (get_internal_api_.IsEmpty()) {
    get_internal_api_.Set(
        isolate, v8::FunctionTemplate::New(
                     isolate, &NativeExtensionBindingsSystem::GetInternalAPI,
                     v8::Local<v8::Value>(), v8::Local<v8::Signature>(), 0,
                     v8::ConstructorBehavior::kThrow));
  }

  // Note: it's a shame we can't delay this (until, say, we knew an API would
  // actually be used), but it's needed for some of our crazier hooks, like
  // web/guest view.
  context->module_system()->SetGetInternalAPIHook(
      get_internal_api_.Get(isolate));
}

void NativeExtensionBindingsSystem::WillReleaseScriptContext(
    ScriptContext* context) {}

void NativeExtensionBindingsSystem::UpdateBindingsForContext(
    ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Local<v8::Context> v8_context = context->v8_context();
  v8::Local<v8::Object> chrome = GetOrCreateChrome(v8_context);
  if (chrome.IsEmpty())
    return;

  BindingsSystemPerContextData* data = GetBindingsDataFromContext(v8_context);
  DCHECK(data);

  const FeatureProvider* api_feature_provider =
      FeatureProvider::GetAPIFeatures();
  base::StringPiece last_accessor;
  for (const auto& map_entry : api_feature_provider->GetAllFeatures()) {
    // If we've already set up an accessor for the immediate property of the
    // chrome object, we don't need to do more.
    if (IsPrefixedAPI(map_entry.first, last_accessor))
      continue;

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

    // We've found an API that's available to the extension. Normally, we will
    // expose this under the name of the feature (e.g., 'tabs'), but in some
    // cases, this will be a prefixed API, such as 'app.runtime'. Find what the
    // property on the chrome object is named, and use that. So in the case of
    // 'app.runtime', we surface a getter for simply 'app'.
    base::StringPiece accessor_name =
        GetFirstDifferentAPIName(map_entry.first, base::StringPiece());
    last_accessor = accessor_name;
    v8::Local<v8::String> api_name =
        gin::StringToSymbol(v8_context->GetIsolate(), accessor_name);
    v8::Maybe<bool> success = chrome->SetAccessor(
        v8_context, api_name, &BindingAccessor, nullptr, api_name);
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
  api_system_.CompleteRequest(request_id, response, error);
}

RequestSender* NativeExtensionBindingsSystem::GetRequestSender() {
  return nullptr;
}

void NativeExtensionBindingsSystem::BindingAccessor(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = info.Holder()->CreationContext();

  // We use info.Data() to store a real name here instead of using the provided
  // one to handle any weirdness from the caller (non-existent strings, etc).
  v8::Local<v8::String> api_name = info.Data().As<v8::String>();
  v8::Local<v8::Object> binding = GetAPIHelper(context, api_name);
  if (!binding.IsEmpty())
    info.GetReturnValue().Set(binding);
}

// static
v8::Local<v8::Object> NativeExtensionBindingsSystem::GetAPIHelper(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> api_name) {
  BindingsSystemPerContextData* data = GetBindingsDataFromContext(context);
  if (!data)
    return v8::Local<v8::Object>();

  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Object> apis;
  if (data->api_object.IsEmpty()) {
    apis = v8::Object::New(isolate);
    data->api_object = v8::Global<v8::Object>(isolate, apis);
  } else {
    apis = data->api_object.Get(isolate);
  }

  v8::Maybe<bool> has_property = apis->HasRealNamedProperty(context, api_name);
  if (!has_property.IsJust())
    return v8::Local<v8::Object>();

  if (has_property.FromJust()) {
    v8::Local<v8::Value> value =
        apis->GetRealNamedProperty(context, api_name).ToLocalChecked();
    DCHECK(value->IsObject());
    return value.As<v8::Object>();
  }

  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  std::string api_name_string;
  CHECK(
      gin::Converter<std::string>::FromV8(isolate, api_name, &api_name_string));

  v8::Local<v8::Object> root_binding = CreateFullBinding(
      context, script_context, &data->bindings_system->api_system_,
      FeatureProvider::GetAPIFeatures(), api_name_string);
  if (root_binding.IsEmpty())
    return v8::Local<v8::Object>();

  v8::Maybe<bool> success =
      apis->CreateDataProperty(context, api_name, root_binding);
  if (!success.IsJust() || !success.FromJust())
    return v8::Local<v8::Object>();

  return root_binding;
}

v8::Local<v8::Object> NativeExtensionBindingsSystem::GetRuntime(
    v8::Local<v8::Context> context) {
  return GetAPIHelper(context,
                      gin::StringToSymbol(context->GetIsolate(), "runtime"));
}

// static
void NativeExtensionBindingsSystem::GetInternalAPI(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK_EQ(1, info.Length());
  CHECK(info[0]->IsString());

  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::String> v8_name = info[0].As<v8::String>();

  BindingsSystemPerContextData* data = GetBindingsDataFromContext(context);
  CHECK(data);

  v8::Local<v8::Object> internal_apis;
  if (data->internal_apis.IsEmpty()) {
    internal_apis = v8::Object::New(isolate);
    data->internal_apis.Reset(isolate, internal_apis);
  } else {
    internal_apis = data->internal_apis.Get(isolate);
  }

  v8::Maybe<bool> has_property =
      internal_apis->HasOwnProperty(context, v8_name);
  if (!has_property.IsJust())
    return;

  if (has_property.FromJust()) {
    // API was already instantiated.
    info.GetReturnValue().Set(
        internal_apis->GetRealNamedProperty(context, v8_name).ToLocalChecked());
    return;
  }

  std::string api_name = gin::V8ToString(info[0]);
  const Feature* feature = FeatureProvider::GetAPIFeature(api_name);
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  if (!feature ||
      !script_context->IsAnyFeatureAvailableToContext(
          *feature, CheckAliasStatus::NOT_ALLOWED)) {
    NOTREACHED();
    return;
  }

  CHECK(feature->IsInternal());

  // We don't need to go through CreateFullBinding here because internal APIs
  // are always acquired through getInternalBinding and specified by full name,
  // rather than through access on the chrome object. So we can just instantiate
  // a binding keyed with any name, even a prefixed one (e.g.
  // 'app.currentWindowInternal').
  v8::Local<v8::Object> api_binding = CreateRootBinding(
      context, script_context, api_name, &data->bindings_system->api_system_);

  if (api_binding.IsEmpty())
    return;

  v8::Maybe<bool> success =
      internal_apis->CreateDataProperty(context, v8_name, api_binding);
  if (!success.IsJust() || !success.FromJust())
    return;

  info.GetReturnValue().Set(api_binding);
}

void NativeExtensionBindingsSystem::SendRequest(
    std::unique_ptr<APIRequestHandler::Request> request,
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

  send_request_ipc_.Run(script_context, params);
}

void NativeExtensionBindingsSystem::OnEventListenerChanged(
    const std::string& event_name,
    binding::EventListenersChanged change,
    v8::Local<v8::Context> context) {
  send_event_listener_ipc_.Run(
      change, ScriptContextSet::GetContextByV8Context(context), event_name);
}

}  // namespace extensions
