// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/native_extension_bindings_system.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_invocation_errors.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/string_source_map.h"
#include "extensions/renderer/test_v8_extension_configuration.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

enum class ItemType {
  EXTENSION,
  PLATFORM_APP,
};

// Creates an extension with the given |name| and |permissions|.
scoped_refptr<Extension> CreateExtension(
    const std::string& name,
    ItemType type,
    const std::vector<std::string>& permissions) {
  DictionaryBuilder manifest;
  manifest.Set("name", name);
  manifest.Set("manifest_version", 2);
  manifest.Set("version", "0.1");
  manifest.Set("description", "test extension");

  if (type == ItemType::PLATFORM_APP) {
    DictionaryBuilder background;
    background.Set("scripts", ListBuilder().Append("test.js").Build());
    manifest.Set(
        "app",
        DictionaryBuilder().Set("background", background.Build()).Build());
  }

  {
    ListBuilder permissions_builder;
    for (const std::string& permission : permissions)
      permissions_builder.Append(permission);
    manifest.Set("permissions", permissions_builder.Build());
  }

  return ExtensionBuilder()
      .SetManifest(manifest.Build())
      .SetLocation(Manifest::INTERNAL)
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

class EventChangeHandler {
 public:
  MOCK_METHOD5(OnChange,
               void(binding::EventListenersChanged,
                    ScriptContext*,
                    const std::string& event_name,
                    const base::DictionaryValue* filter,
                    bool was_manual));
};

// Returns true if the value specified by |property| exists in the given
// context.
bool PropertyExists(v8::Local<v8::Context> context,
                    base::StringPiece property) {
  v8::Local<v8::Value> value = V8ValueFromScriptSource(context, property);
  EXPECT_FALSE(value.IsEmpty());
  return !value->IsUndefined();
};

}  // namespace

class NativeExtensionBindingsSystemUnittest : public APIBindingTest {
 public:
  NativeExtensionBindingsSystemUnittest() {}
  ~NativeExtensionBindingsSystemUnittest() override {}

 protected:
  using MockEventChangeHandler = ::testing::StrictMock<EventChangeHandler>;

  v8::ExtensionConfiguration* GetV8ExtensionConfiguration() override {
    return TestV8ExtensionConfiguration::GetConfiguration();
  }

  void SetUp() override {
    script_context_set_ = base::MakeUnique<ScriptContextSet>(&extension_ids_);
    bindings_system_ = base::MakeUnique<NativeExtensionBindingsSystem>(
        base::Bind(&NativeExtensionBindingsSystemUnittest::MockSendRequestIPC,
                   base::Unretained(this)),
        base::Bind(&NativeExtensionBindingsSystemUnittest::MockSendListenerIPC,
                   base::Unretained(this)));
    APIBindingTest::SetUp();
  }

  void TearDown() override {
    event_change_handler_.reset();
    // Dispose all contexts now so we call WillReleaseScriptContext() on the
    // bindings system.
    DisposeAllContexts();

    // ScriptContexts are deleted asynchronously by the ScriptContextSet, so we
    // need spin here to ensure we don't leak. See also
    // ScriptContextSet::Remove().
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(raw_script_contexts_.empty());
    script_context_set_.reset();
    bindings_system_.reset();
    APIBindingTest::TearDown();
  }

  void MockSendRequestIPC(ScriptContext* context,
                          const ExtensionHostMsg_Request_Params& params,
                          binding::RequestThread thread) {
    last_params_.name = params.name;
    last_params_.arguments.Swap(params.arguments.CreateDeepCopy().get());
    last_params_.extension_id = params.extension_id;
    last_params_.source_url = params.source_url;
    last_params_.request_id = params.request_id;
    last_params_.has_callback = params.has_callback;
    last_params_.user_gesture = params.user_gesture;
    last_params_.worker_thread_id = params.worker_thread_id;
    last_params_.service_worker_version_id = params.service_worker_version_id;
  }

  void MockSendListenerIPC(binding::EventListenersChanged changed,
                           ScriptContext* context,
                           const std::string& event_name,
                           const base::DictionaryValue* filter,
                           bool was_manual) {
    if (event_change_handler_) {
      event_change_handler_->OnChange(changed, context, event_name, filter,
                                      was_manual);
    }
  }

  ScriptContext* CreateScriptContext(v8::Local<v8::Context> v8_context,
                                     Extension* extension,
                                     Feature::Context context_type) {
    auto script_context = base::MakeUnique<ScriptContext>(
        v8_context, nullptr, extension, context_type, extension, context_type);
    script_context->set_module_system(
        base::MakeUnique<ModuleSystem>(script_context.get(), source_map()));
    ScriptContext* raw_script_context = script_context.get();
    raw_script_contexts_.push_back(raw_script_context);
    script_context_set_->AddForTesting(std::move(script_context));
    bindings_system_->DidCreateScriptContext(raw_script_context);
    return raw_script_context;
  }

  void OnWillDisposeContext(v8::Local<v8::Context> context) override {
    auto iter =
        std::find_if(raw_script_contexts_.begin(), raw_script_contexts_.end(),
                     [context](ScriptContext* script_context) {
                       return script_context->v8_context() == context;
                     });
    ASSERT_TRUE(iter != raw_script_contexts_.end());
    bindings_system_->WillReleaseScriptContext(*iter);
    script_context_set_->Remove(*iter);
    raw_script_contexts_.erase(iter);
  }

  void RegisterExtension(const ExtensionId& id) { extension_ids_.insert(id); }

  void InitEventChangeHandler() {
    event_change_handler_ = base::MakeUnique<MockEventChangeHandler>();
  }

  NativeExtensionBindingsSystem* bindings_system() {
    return bindings_system_.get();
  }
  const ExtensionHostMsg_Request_Params& last_params() { return last_params_; }
  StringSourceMap* source_map() { return &source_map_; }
  MockEventChangeHandler* event_change_handler() {
    return event_change_handler_.get();
  }

 private:
  ExtensionIdSet extension_ids_;
  std::unique_ptr<ScriptContextSet> script_context_set_;
  std::vector<ScriptContext*> raw_script_contexts_;
  std::unique_ptr<NativeExtensionBindingsSystem> bindings_system_;

  ExtensionHostMsg_Request_Params last_params_;
  std::unique_ptr<MockEventChangeHandler> event_change_handler_;

  StringSourceMap source_map_;

  DISALLOW_COPY_AND_ASSIGN(NativeExtensionBindingsSystemUnittest);
};

TEST_F(NativeExtensionBindingsSystemUnittest, Basic) {
  scoped_refptr<Extension> extension = CreateExtension(
      "foo", ItemType::EXTENSION, {"idle", "power", "webRequest"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // chrome.idle.queryState should exist.
  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  v8::Local<v8::Value> idle = GetPropertyFromObject(
      v8::Local<v8::Object>::Cast(chrome), context, "idle");
  ASSERT_FALSE(idle.IsEmpty());
  ASSERT_TRUE(idle->IsObject());

  v8::Local<v8::Object> idle_object = v8::Local<v8::Object>::Cast(idle);
  v8::Local<v8::Value> idle_query_state =
      GetPropertyFromObject(idle_object, context, "queryState");
  ASSERT_FALSE(idle_query_state.IsEmpty());

  EXPECT_EQ(ReplaceSingleQuotes(
                "{'ACTIVE':'active','IDLE':'idle','LOCKED':'locked'}"),
            GetStringPropertyFromObject(idle_object, context, "IdleState"));

  {
    // Try calling the function with an invalid invocation - an error should be
    // thrown.
    const char kCallIdleQueryStateInvalid[] =
        "(function() {\n"
        "  chrome.idle.queryState('foo', function(state) {\n"
        "    this.responseState = state;\n"
        "  });\n"
        "});";
    v8::Local<v8::Function> function =
        FunctionFromString(context, kCallIdleQueryStateInvalid);
    ASSERT_FALSE(function.IsEmpty());
    RunFunctionAndExpectError(
        function, context, 0, nullptr,
        "Uncaught TypeError: " +
            api_errors::InvocationError(
                "idle.queryState",
                "integer detectionIntervalInSeconds, function callback",
                api_errors::ArgumentError(
                    "detectionIntervalInSeconds",
                    api_errors::InvalidType(api_errors::kTypeInteger,
                                            api_errors::kTypeString))));
  }

  {
    // Call the function correctly.
    const char kCallIdleQueryState[] =
        "(function() {\n"
        "  chrome.idle.queryState(30, function(state) {\n"
        "    this.responseState = state;\n"
        "  });\n"
        "});";

    v8::Local<v8::Function> call_idle_query_state =
        FunctionFromString(context, kCallIdleQueryState);
    RunFunctionOnGlobal(call_idle_query_state, context, 0, nullptr);
  }

  // Validate the params that would be sent to the browser.
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.queryState", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  EXPECT_TRUE(last_params().has_callback);
  EXPECT_TRUE(
      last_params().arguments.Equals(ListValueFromString("[30]").get()));

  // Respond and validate.
  bindings_system()->HandleResponse(last_params().request_id, true,
                                    *ListValueFromString("['active']"),
                                    std::string());

  std::unique_ptr<base::Value> result_value = GetBaseValuePropertyFromObject(
      context->Global(), context, "responseState");
  ASSERT_TRUE(result_value);
  EXPECT_EQ("\"active\"", ValueToString(*result_value));

  // Sanity-check that another API also exists as expected.
  v8::Local<v8::Value> power_api =
      V8ValueFromScriptSource(context, "chrome.power");
  ASSERT_FALSE(power_api.IsEmpty());
  ASSERT_TRUE(power_api->IsObject());
  v8::Local<v8::Value> request_keep_awake = GetPropertyFromObject(
      power_api.As<v8::Object>(), context, "requestKeepAwake");
  ASSERT_FALSE(request_keep_awake.IsEmpty());
  EXPECT_TRUE(request_keep_awake->IsFunction());

  // Test properties exposed on the API object itself.
  v8::Local<v8::Value> web_request =
      V8ValueFromScriptSource(context, "chrome.webRequest");
  ASSERT_FALSE(web_request.IsEmpty());
  ASSERT_TRUE(web_request->IsObject());
  EXPECT_EQ("20", GetStringPropertyFromObject(
                      web_request.As<v8::Object>(), context,
                      "MAX_HANDLER_BEHAVIOR_CHANGED_CALLS_PER_10_MINUTES"));
}

TEST_F(NativeExtensionBindingsSystemUnittest, Events) {
  scoped_refptr<Extension> extension =
      CreateExtension("foo", ItemType::EXTENSION, {"idle", "power"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  {
    const char kAddStateChangedListeners[] =
        "(function() {\n"
        "  chrome.idle.onStateChanged.addListener(function() {\n"
        "    this.didThrow = true;\n"
        "    throw new Error('Error!!!');\n"
        "  });\n"
        "  chrome.idle.onStateChanged.addListener(function(newState) {\n"
        "    this.newState = newState;\n"
        "  });\n"
        "});";

    v8::Local<v8::Function> add_listeners =
        FunctionFromString(context, kAddStateChangedListeners);
    RunFunctionOnGlobal(add_listeners, context, 0, nullptr);
  }

  bindings_system()->DispatchEventInContext(
      "idle.onStateChanged", ListValueFromString("['idle']").get(), nullptr,
      script_context);
  EXPECT_EQ("\"idle\"", GetStringPropertyFromObject(context->Global(), context,
                                                    "newState"));
  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "didThrow"));
}

// Tests that referencing the same API multiple times returns the same object;
// i.e. chrome.foo === chrome.foo.
TEST_F(NativeExtensionBindingsSystemUnittest, APIObjectsAreEqual) {
  scoped_refptr<Extension> extension =
      CreateExtension("foo", ItemType::EXTENSION, {"idle"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> first_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(first_idle_object.IsEmpty());
  EXPECT_TRUE(first_idle_object->IsObject());
  EXPECT_FALSE(first_idle_object->IsUndefined());
  v8::Local<v8::Value> second_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  EXPECT_TRUE(first_idle_object == second_idle_object);
}

// Tests that referencing APIs after the context data is disposed is safe (and
// returns undefined).
TEST_F(NativeExtensionBindingsSystemUnittest,
       ReferencingAPIAfterDisposingContext) {
  scoped_refptr<Extension> extension =
      CreateExtension("foo", ItemType::EXTENSION, {"idle", "power"});

  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> first_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(first_idle_object.IsEmpty());
  EXPECT_TRUE(first_idle_object->IsObject());

  DisposeContext(context);

  // Check an API that was instantiated....
  v8::Local<v8::Value> second_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(second_idle_object.IsEmpty());
  EXPECT_TRUE(second_idle_object->IsUndefined());
  // ... and also one that wasn't.
  v8::Local<v8::Value> power_object =
      V8ValueFromScriptSource(context, "chrome.power");
  ASSERT_FALSE(power_object.IsEmpty());
  EXPECT_TRUE(power_object->IsUndefined());
}

// Tests that traditional custom bindings can be used with the native bindings
// system.
TEST_F(NativeExtensionBindingsSystemUnittest, TestBridgingToJSCustomBindings) {
  // Custom binding code. This basically utilizes the interface in binding.js in
  // order to test backwards compatibility.
  const char kCustomBinding[] =
      "apiBridge.registerCustomHook((api, extensionId, contextType) => {\n"
      "  api.apiFunctions.setHandleRequest('queryState',\n"
      "                                    (time, callback) => {\n"
      "    this.timeArg = time;\n"
      "    callback('active');\n"
      "  });\n"
      "  api.apiFunctions.setUpdateArgumentsPreValidate(\n"
      "      'setDetectionInterval', (interval) => {\n"
      "    this.intervalArg = interval;\n"
      "    return [50];\n"
      "  });\n"
      "  this.hookedExtensionId = extensionId;\n"
      "  this.hookedContextType = contextType;\n"
      "  api.compiledApi.hookedApiProperty = 'someProperty';\n"
      "});\n";

  source_map()->RegisterModule("idle", kCustomBinding);

  scoped_refptr<Extension> extension =
      CreateExtension("foo", ItemType::EXTENSION, {"idle"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  {
    // Call the function correctly.
    const char kCallIdleQueryState[] =
        "(function() {\n"
        "  chrome.idle.queryState(30, function(state) {\n"
        "    this.responseState = state;\n"
        "  });\n"
        "});";

    v8::Local<v8::Function> call_idle_query_state =
        FunctionFromString(context, kCallIdleQueryState);
    RunFunctionOnGlobal(call_idle_query_state, context, 0, nullptr);
  }

  // To start, check that the properties we set when running the hooks are
  // correct. We do this after calling the function because the API objects (and
  // thus the hooks) are set up lazily.
  v8::Local<v8::Object> global = context->Global();
  EXPECT_EQ(base::StringPrintf("\"%s\"", extension->id().c_str()),
            GetStringPropertyFromObject(global, context, "hookedExtensionId"));
  EXPECT_EQ("\"BLESSED_EXTENSION\"",
            GetStringPropertyFromObject(global, context, "hookedContextType"));
  v8::Local<v8::Value> idle_api =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(idle_api.IsEmpty());
  ASSERT_TRUE(idle_api->IsObject());
  EXPECT_EQ("\"someProperty\"",
            GetStringPropertyFromObject(idle_api.As<v8::Object>(), context,
                                        "hookedApiProperty"));

  // Next, we need to check two pieces: first, that the custom handler was
  // called with the proper arguments....
  EXPECT_EQ("30", GetStringPropertyFromObject(global, context, "timeArg"));

  // ...and second, that the callback was called with the proper result.
  EXPECT_EQ("\"active\"",
            GetStringPropertyFromObject(global, context, "responseState"));

  // Test the updateArgumentsPreValidate hook.
  {
    // Call the function correctly.
    const char kCallIdleSetInterval[] =
        "(function() {\n"
        "  chrome.idle.setDetectionInterval(20);\n"
        "});";

    v8::Local<v8::Function> call_idle_set_interval =
        FunctionFromString(context, kCallIdleSetInterval);
    RunFunctionOnGlobal(call_idle_set_interval, context, 0, nullptr);
  }

  // Since we don't have a custom request handler, the hook should have only
  // updated the arguments. The request then should have gone to the browser
  // normally.
  EXPECT_EQ("20", GetStringPropertyFromObject(global, context, "intervalArg"));
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.setDetectionInterval", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  EXPECT_FALSE(last_params().has_callback);
  EXPECT_TRUE(
      last_params().arguments.Equals(ListValueFromString("[50]").get()));
}

TEST_F(NativeExtensionBindingsSystemUnittest, TestSendRequestHook) {
  // Custom binding code. This basically utilizes the interface in binding.js in
  // order to test backwards compatibility.
  const char kCustomBinding[] =
      "apiBridge.registerCustomHook((api) => {\n"
      "  api.apiFunctions.setHandleRequest('queryState',\n"
      "                                    (time, callback) => {\n"
      "    bindingUtil.sendRequest('idle.queryState', [time, callback],\n"
      "                            undefined, undefined);\n"
      "  });\n"
      "});\n";

  source_map()->RegisterModule("idle", kCustomBinding);

  scoped_refptr<Extension> extension =
      CreateExtension("foo", ItemType::EXTENSION, {"idle"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  {
    // Call the function correctly.
    const char kCallIdleQueryState[] =
        "(function() { chrome.idle.queryState(30, function() {}); });";

    v8::Local<v8::Function> call_idle_query_state =
        FunctionFromString(context, kCallIdleQueryState);
    RunFunctionOnGlobal(call_idle_query_state, context, 0, nullptr);
  }
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.queryState", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  EXPECT_TRUE(last_params().has_callback);
  EXPECT_TRUE(
      last_params().arguments.Equals(ListValueFromString("[30]").get()));
}

// Tests that we can notify the browser as event listeners are added or removed.
// Note: the notification logic is tested more thoroughly in the APIEventHandler
// unittests.
TEST_F(NativeExtensionBindingsSystemUnittest, TestEventRegistration) {
  InitEventChangeHandler();
  scoped_refptr<Extension> extension =
      CreateExtension("foo", ItemType::EXTENSION, {"idle", "power"});

  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // Add a new event listener. We should be notified of the change.
  const char kEventName[] = "idle.onStateChanged";
  v8::Local<v8::Function> listener =
      FunctionFromString(context, "(function() {})");
  const char kAddListener[] =
      "(function(listener) {\n"
      "  chrome.idle.onStateChanged.addListener(listener);\n"
      "});";
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kAddListener);
  EXPECT_CALL(*event_change_handler(),
              OnChange(binding::EventListenersChanged::HAS_LISTENERS,
                       script_context, kEventName, nullptr, true))
      .Times(1);
  v8::Local<v8::Value> argv[] = {listener};
  RunFunction(add_listener, context, arraysize(argv), argv);
  ::testing::Mock::VerifyAndClearExpectations(event_change_handler());
  EXPECT_TRUE(bindings_system()->HasEventListenerInContext(
      "idle.onStateChanged", script_context));

  // Remove the event listener. We should be notified again.
  const char kRemoveListener[] =
      "(function(listener) {\n"
      "  chrome.idle.onStateChanged.removeListener(listener);\n"
      "});";
  EXPECT_CALL(*event_change_handler(),
              OnChange(binding::EventListenersChanged::NO_LISTENERS,
                       script_context, kEventName, nullptr, true))
      .Times(1);
  v8::Local<v8::Function> remove_listener =
      FunctionFromString(context, kRemoveListener);
  RunFunction(remove_listener, context, arraysize(argv), argv);
  ::testing::Mock::VerifyAndClearExpectations(event_change_handler());
  EXPECT_FALSE(bindings_system()->HasEventListenerInContext(
      "idle.onStateChanged", script_context));
}

TEST_F(NativeExtensionBindingsSystemUnittest,
       TestPrefixedApiEventsAndAppBinding) {
  InitEventChangeHandler();
  scoped_refptr<Extension> app = CreateExtension("foo", ItemType::PLATFORM_APP,
                                                 std::vector<std::string>());
  EXPECT_TRUE(app->is_platform_app());
  RegisterExtension(app->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, app.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(app->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // The 'chrome.app' object should have 'runtime' and 'window' entries, but
  // not the internal 'currentWindowInternal' object.
  v8::Local<v8::Value> app_binding_keys =
      V8ValueFromScriptSource(context,
                              "JSON.stringify(Object.keys(chrome.app));");
  ASSERT_FALSE(app_binding_keys.IsEmpty());
  ASSERT_TRUE(app_binding_keys->IsString());
  EXPECT_EQ("[\"runtime\",\"window\"]",
            gin::V8ToString(app_binding_keys));

  const char kUseAppRuntime[] =
      "(function() {\n"
      "  chrome.app.runtime.onLaunched.addListener(function() {});\n"
      "});";
  v8::Local<v8::Function> use_app_runtime =
      FunctionFromString(context, kUseAppRuntime);
  EXPECT_CALL(*event_change_handler(),
              OnChange(binding::EventListenersChanged::HAS_LISTENERS,
                       script_context, "app.runtime.onLaunched", nullptr, true))
      .Times(1);
  RunFunctionOnGlobal(use_app_runtime, context, 0, nullptr);
  ::testing::Mock::VerifyAndClearExpectations(event_change_handler());
}

TEST_F(NativeExtensionBindingsSystemUnittest,
       TestPrefixedApiMethodsAndSystemBinding) {
  scoped_refptr<Extension> extension =
      CreateExtension("foo", ItemType::EXTENSION, {"system.cpu"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // The system.cpu object should exist, but system.network should not (as the
  // extension didn't request permission to it).
  v8::Local<v8::Value> system_cpu =
      V8ValueFromScriptSource(context, "chrome.system.cpu");
  ASSERT_FALSE(system_cpu.IsEmpty());
  EXPECT_TRUE(system_cpu->IsObject());
  EXPECT_FALSE(system_cpu->IsUndefined());

  v8::Local<v8::Value> system_network =
      V8ValueFromScriptSource(context, "chrome.system.network");
  ASSERT_FALSE(system_network.IsEmpty());
  EXPECT_TRUE(system_network->IsUndefined());

  const char kUseSystemCpu[] =
      "(function() {\n"
      "  chrome.system.cpu.getInfo(function() {})\n"
      "});";
  v8::Local<v8::Function> use_system_cpu =
      FunctionFromString(context, kUseSystemCpu);
  RunFunctionOnGlobal(use_system_cpu, context, 0, nullptr);

  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("system.cpu.getInfo", last_params().name);
  EXPECT_TRUE(last_params().has_callback);
}

TEST_F(NativeExtensionBindingsSystemUnittest, TestLastError) {
  scoped_refptr<Extension> extension =
      CreateExtension("foo", ItemType::EXTENSION, {"idle", "power"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  const char kCallFunction[] =
      "(function() {\n"
      "  chrome.idle.queryState(30, function(state) {\n"
      "    if (chrome.runtime.lastError)\n"
      "      this.lastErrorMessage = chrome.runtime.lastError.message;\n"
      "  });\n"
      "});";
  v8::Local<v8::Function> function = FunctionFromString(context, kCallFunction);
  ASSERT_FALSE(function.IsEmpty());
  RunFunctionOnGlobal(function, context, 0, nullptr);

  // Validate the params that would be sent to the browser.
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.queryState", last_params().name);

  int first_request_id = last_params().request_id;
  // Respond with an error.
  bindings_system()->HandleResponse(last_params().request_id, false,
                                    base::ListValue(), "Some API Error");
  EXPECT_EQ("\"Some API Error\"",
            GetStringPropertyFromObject(context->Global(), context,
                                        "lastErrorMessage"));

  // Test responding with a failure, but no set error.
  RunFunctionOnGlobal(function, context, 0, nullptr);
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.queryState", last_params().name);
  EXPECT_NE(first_request_id, last_params().request_id);

  bindings_system()->HandleResponse(last_params().request_id, false,
                                    base::ListValue(), std::string());
  EXPECT_EQ("\"Unknown error.\"",
            GetStringPropertyFromObject(context->Global(), context,
                                        "lastErrorMessage"));
}

TEST_F(NativeExtensionBindingsSystemUnittest, TestCustomProperties) {
  scoped_refptr<Extension> extension =
      CreateExtension("storage extension", ItemType::EXTENSION, {"storage"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> storage =
      V8ValueFromScriptSource(context, "chrome.storage");
  ASSERT_FALSE(storage.IsEmpty());
  ASSERT_TRUE(storage->IsObject());

  v8::Local<v8::Value> local =
      GetPropertyFromObject(storage.As<v8::Object>(), context, "local");
  ASSERT_FALSE(local.IsEmpty());
  ASSERT_TRUE(local->IsObject());

  v8::Local<v8::Object> local_object = local.As<v8::Object>();
  const std::vector<std::string> kKeys = {"get", "set", "remove", "clear",
                                          "getBytesInUse"};
  for (const auto& key : kKeys) {
    v8::Local<v8::String> v8_key = gin::StringToV8(isolate(), key);
    EXPECT_TRUE(local_object->HasOwnProperty(context, v8_key).FromJust())
        << key;
  }
}

// Ensure that different contexts have different API objects.
TEST_F(NativeExtensionBindingsSystemUnittest,
       CheckDifferentContextsHaveDifferentAPIObjects) {
  scoped_refptr<Extension> extension =
      CreateExtension("extension", ItemType::EXTENSION, {"idle"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  ScriptContext* script_context_a = CreateScriptContext(
      context_a, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context_a->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context_a);

  ScriptContext* script_context_b = CreateScriptContext(
      context_b, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context_b->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context_b);

  auto check_properties_inequal = [](v8::Local<v8::Context> context_a,
                                     v8::Local<v8::Context> context_b,
                                     base::StringPiece property) {
    v8::Local<v8::Value> value_a = V8ValueFromScriptSource(context_a, property);
    v8::Local<v8::Value> value_b = V8ValueFromScriptSource(context_b, property);
    EXPECT_FALSE(value_a.IsEmpty()) << property;
    EXPECT_FALSE(value_b.IsEmpty()) << property;
    EXPECT_NE(value_a, value_b) << property;
  };

  check_properties_inequal(context_a, context_b, "chrome");
  check_properties_inequal(context_a, context_b, "chrome.idle");
  check_properties_inequal(context_a, context_b, "chrome.idle.onStateChanged");
}

// Tests that API methods and events that are conditionally available based on
// context are properly present or absent from the API object.
TEST_F(NativeExtensionBindingsSystemUnittest,
       CheckRestrictedFeaturesBasedOnContext) {
  scoped_refptr<Extension> extension =
      CreateExtension("extension", ItemType::EXTENSION, {"idle"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> blessed_context = MainContext();
  v8::Local<v8::Context> webpage_context = AddContext();

  // Create two contexts - a blessed extension context and a normal web page
  // context.
  ScriptContext* blessed_script_context = CreateScriptContext(
      blessed_context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  blessed_script_context->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(blessed_script_context);

  ScriptContext* webpage_script_context =
      CreateScriptContext(webpage_context, nullptr, Feature::WEB_PAGE_CONTEXT);
  webpage_script_context->set_url(GURL("http://example.com"));
  bindings_system()->UpdateBindingsForContext(webpage_script_context);

  // Check that properties are correctly restricted. The blessed context should
  // have access to the whole runtime API, but the webpage should only have
  // access to sendMessage.
  const char kSendMessage[] = "chrome.runtime.sendMessage";
  const char kGetUrl[] = "chrome.runtime.getURL";
  const char kOnMessage[] = "chrome.runtime.onMessage";
  EXPECT_TRUE(PropertyExists(blessed_context, kSendMessage));
  EXPECT_TRUE(PropertyExists(blessed_context, kGetUrl));
  EXPECT_TRUE(PropertyExists(blessed_context, kOnMessage));
  EXPECT_TRUE(PropertyExists(webpage_context, kSendMessage));
  EXPECT_FALSE(PropertyExists(webpage_context, kGetUrl));
  EXPECT_FALSE(PropertyExists(webpage_context, kOnMessage));
}

// Tests behavior when script sets window.chrome to be various things.
TEST_F(NativeExtensionBindingsSystemUnittest, TestUsingOtherChromeObjects) {
  scoped_refptr<Extension> extension = CreateExtension(
      "extension", ItemType::EXTENSION, std::vector<std::string>());
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  ScriptContext* script_context_a = CreateScriptContext(
      context_a, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context_a->set_url(extension->url());
  ScriptContext* script_context_b = CreateScriptContext(
      context_b, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context_b->set_url(extension->url());

  auto check_runtime = [this, context_a, context_b, script_context_a,
                        script_context_b](bool expect_b_has_runtime) {
    bindings_system()->UpdateBindingsForContext(script_context_a);
    bindings_system()->UpdateBindingsForContext(script_context_b);

    const char kRuntime[] = "chrome.runtime";
    // chrome.runtime should always exist in context a - we only mess with
    // context b.
    EXPECT_TRUE(PropertyExists(context_a, kRuntime));
    EXPECT_EQ(expect_b_has_runtime, PropertyExists(context_b, kRuntime));
  };

  // By default, runtime should exist in both contexts (since both have access
  // to the API).
  check_runtime(true);

  {
    v8::Context::Scope scope(context_a);
    v8::Local<v8::Object> fake_chrome = v8::Object::New(isolate());
    EXPECT_EQ(context_a, fake_chrome->CreationContext());
    context_b->Global()
        ->Set(context_b, gin::StringToSymbol(isolate(), "chrome"), fake_chrome)
        .ToChecked();
  }
  // context_b has a chrome object that was created in a different context
  // (context_a), so we shouldn't have used it. This can legitimately happen in
  // the case of a parent frame modifying a child frame's window.chrome.
  check_runtime(false);

  {
    v8::Context::Scope scope(context_b);
    v8::Local<v8::Object> fake_chrome = v8::Object::New(isolate());
    EXPECT_EQ(context_b, fake_chrome->CreationContext());
    context_b->Global()
        ->Set(context_b, gin::StringToSymbol(isolate(), "chrome"), fake_chrome)
        .ToChecked();
  }
  // When the chrome object is created in the same context (context_b), that
  // object will be used.
  check_runtime(true);

  {
    v8::Context::Scope scope(context_b);
    v8::Local<v8::Boolean> fake_chrome = v8::Boolean::New(isolate(), true);
    context_b->Global()
        ->Set(context_b, gin::StringToSymbol(isolate(), "chrome"), fake_chrome)
        .ToChecked();
  }
  // A non-object chrome shouldn't be used.
  check_runtime(false);
}

// Tests updating a context's bindings after adding or removing permissions.
TEST_F(NativeExtensionBindingsSystemUnittest, TestUpdatingPermissions) {
  scoped_refptr<Extension> extension =
      CreateExtension("extension", ItemType::EXTENSION, {"idle"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context);

  // To start, chrome.idle should be available.
  v8::Local<v8::Value> initial_idle =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(initial_idle.IsEmpty());
  EXPECT_TRUE(initial_idle->IsObject());

  {
    // chrome.power should not be defined.
    v8::Local<v8::Value> power =
        V8ValueFromScriptSource(context, "chrome.power");
    ASSERT_FALSE(power.IsEmpty());
    EXPECT_TRUE(power->IsUndefined());
  }

  // Remove all permissions (`idle`).
  extension->permissions_data()->SetPermissions(
      base::MakeUnique<PermissionSet>(), base::MakeUnique<PermissionSet>());

  bindings_system()->UpdateBindingsForContext(script_context);
  {
    // TODO(devlin): Neither the native nor JS bindings systems clear the
    // property on the chrome object when an API is no longer available. This
    // seems unexpected, but warrants further investigation before changing
    // behavior. It can be complicated by the fact that chrome.idle may not be
    // the same chrome.idle the system instantiated, or may have additional
    // properties.
    // v8::Local<v8::Value> idle =
    //     V8ValueFromScriptSource(context, "chrome.idle");
    // ASSERT_FALSE(idle.IsEmpty());
    // EXPECT_TRUE(idle->IsUndefined());

    // chrome.power should still be undefined.
    v8::Local<v8::Value> power =
        V8ValueFromScriptSource(context, "chrome.power");
    ASSERT_FALSE(power.IsEmpty());
    EXPECT_TRUE(power->IsUndefined());
  }

  v8::Local<v8::Function> run_idle = FunctionFromString(
      context, "(function(idle) { idle.queryState(30, function() {}); })");
  {
    // Trying to run a chrome.idle function should fail.
    v8::Local<v8::Value> args[] = {initial_idle};
    RunFunctionAndExpectError(
        run_idle, context, arraysize(args), args,
        "Uncaught Error: 'idle.queryState' is not available in this context.");
    EXPECT_TRUE(last_params().name.empty());
  }

  {
    // Add back the `idle` permission, and also add `power`.
    APIPermissionSet apis;
    apis.insert(APIPermission::kPower);
    apis.insert(APIPermission::kIdle);
    extension->permissions_data()->SetPermissions(
        base::MakeUnique<PermissionSet>(apis, ManifestPermissionSet(),
                                        URLPatternSet(), URLPatternSet()),
        base::MakeUnique<PermissionSet>());
    bindings_system()->UpdateBindingsForContext(script_context);
  }

  {
    // Both chrome.idle and chrome.power should be defined.
    v8::Local<v8::Value> idle = V8ValueFromScriptSource(context, "chrome.idle");
    ASSERT_FALSE(idle.IsEmpty());
    EXPECT_TRUE(idle->IsObject());

    v8::Local<v8::Value> power =
        V8ValueFromScriptSource(context, "chrome.power");
    ASSERT_FALSE(power.IsEmpty());
    EXPECT_TRUE(power->IsObject());
  }

  {
    // Trying to run a chrome.idle function should now succeed.
    v8::Local<v8::Value> args[] = {initial_idle};
    RunFunction(run_idle, context, arraysize(args), args);
    EXPECT_EQ("idle.queryState", last_params().name);
  }
}

TEST_F(NativeExtensionBindingsSystemUnittest, UnmanagedEvents) {
  InitEventChangeHandler();

  scoped_refptr<Extension> extension =
      CreateExtension("foo", ItemType::EXTENSION, std::vector<std::string>());
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  const char kAddListeners[] =
      "(function() {\n"
      "  chrome.runtime.onMessage.addListener(function() {});\n"
      "  chrome.runtime.onConnect.addListener(function() {});\n"
      "});";

  v8::Local<v8::Function> add_listeners =
      FunctionFromString(context, kAddListeners);
  RunFunctionOnGlobal(add_listeners, context, 0, nullptr);

  // We should have no notifications for event listeners added (since the
  // mock is a strict mock, this will fail if anything was called).
  ::testing::Mock::VerifyAndClearExpectations(event_change_handler());
}

}  // namespace extensions
