// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/module_system_test.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/feature_switch.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/logging_native_handler.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/string_source_map.h"
#include "extensions/renderer/test_v8_extension_configuration.h"
#include "extensions/renderer/utils_native_handler.h"
#include "gin/converter.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {
namespace {

class FailsOnException : public ModuleSystem::ExceptionHandler {
 public:
  FailsOnException() : ModuleSystem::ExceptionHandler(nullptr) {}
  void HandleUncaughtException(const v8::TryCatch& try_catch) override {
    FAIL() << "Uncaught exception: " << CreateExceptionString(try_catch);
  }
};

class GetAPINatives : public ObjectBackedNativeHandler {
 public:
  GetAPINatives(ScriptContext* context,
                NativeExtensionBindingsSystem* bindings_system)
      : ObjectBackedNativeHandler(context) {
    DCHECK_EQ(FeatureSwitch::native_crx_bindings()->IsEnabled(),
              !!bindings_system);

    auto get_api = [](ScriptContext* context,
                      NativeExtensionBindingsSystem* bindings_system,
                      const v8::FunctionCallbackInfo<v8::Value>& args) {
      CHECK_EQ(1, args.Length());
      CHECK(args[0]->IsString());
      std::string api_name = gin::V8ToString(args[0]);
      v8::Local<v8::Object> api;
      if (bindings_system) {
        api = bindings_system->GetAPIObjectForTesting(context, api_name);
      } else {
        v8::Local<v8::Object> full_binding;
        CHECK(
            context->module_system()->Require(api_name).ToLocal(&full_binding))
            << "Failed to get: " << api_name;
        v8::Local<v8::Value> api_value;
        CHECK(full_binding
                  ->Get(context->v8_context(),
                        gin::StringToSymbol(context->isolate(), "binding"))
                  .ToLocal(&api_value))
            << "Failed to get: " << api_name;
        CHECK(api_value->IsObject()) << "Failed to get: " << api_name;
        api = api_value.As<v8::Object>();
      }
      args.GetReturnValue().Set(api);
    };

    RouteFunction("get", base::Bind(get_api, context, bindings_system));
  }
};

}  // namespace

// Native JS functions for doing asserts.
class ModuleSystemTestEnvironment::AssertNatives
    : public ObjectBackedNativeHandler {
 public:
  explicit AssertNatives(ScriptContext* context)
      : ObjectBackedNativeHandler(context),
        assertion_made_(false),
        failed_(false) {
    RouteFunction(
        "AssertTrue",
        base::Bind(&AssertNatives::AssertTrue, base::Unretained(this)));
    RouteFunction(
        "AssertFalse",
        base::Bind(&AssertNatives::AssertFalse, base::Unretained(this)));
  }

  bool assertion_made() { return assertion_made_; }
  bool failed() { return failed_; }

  void AssertTrue(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK_EQ(1, args.Length());
    assertion_made_ = true;
    failed_ = failed_ || !args[0]->ToBoolean(args.GetIsolate())->Value();
  }

  void AssertFalse(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK_EQ(1, args.Length());
    assertion_made_ = true;
    failed_ = failed_ || args[0]->ToBoolean(args.GetIsolate())->Value();
  }

 private:
  bool assertion_made_;
  bool failed_;
};

ModuleSystemTestEnvironment::ModuleSystemTestEnvironment(
    v8::Isolate* isolate,
    ScriptContextSet* context_set)
    : isolate_(isolate),
      context_holder_(new gin::ContextHolder(isolate_)),
      handle_scope_(isolate_),
      context_set_(context_set),
      source_map_(new StringSourceMap()) {
  context_holder_->SetContext(v8::Context::New(
      isolate, TestV8ExtensionConfiguration::GetConfiguration()));

  {
    auto context =
        base::MakeUnique<ScriptContext>(context_holder_->context(),
                                        nullptr,  // WebFrame
                                        nullptr,  // Extension
                                        Feature::BLESSED_EXTENSION_CONTEXT,
                                        nullptr,  // Effective Extension
                                        Feature::BLESSED_EXTENSION_CONTEXT);
    context_ = context.get();
    context_set_->AddForTesting(std::move(context));
  }

  context_->v8_context()->Enter();
  assert_natives_ = new AssertNatives(context_);

  if (FeatureSwitch::native_crx_bindings()->IsEnabled())
    bindings_system_ = base::MakeUnique<NativeExtensionBindingsSystem>(nullptr);

  {
    std::unique_ptr<ModuleSystem> module_system(
        new ModuleSystem(context_, source_map_.get()));
    context_->set_module_system(std::move(module_system));
  }
  ModuleSystem* module_system = context_->module_system();
  module_system->RegisterNativeHandler(
      "assert", std::unique_ptr<NativeHandler>(assert_natives_));
  module_system->RegisterNativeHandler(
      "logging",
      std::unique_ptr<NativeHandler>(new LoggingNativeHandler(context_)));
  module_system->RegisterNativeHandler(
      "utils",
      std::unique_ptr<NativeHandler>(new UtilsNativeHandler(context_)));
  module_system->RegisterNativeHandler(
      "apiGetter",
      base::MakeUnique<GetAPINatives>(context_, bindings_system_.get()));
  module_system->SetExceptionHandlerForTest(
      std::unique_ptr<ModuleSystem::ExceptionHandler>(new FailsOnException));

  if (bindings_system_) {
    bindings_system_->DidCreateScriptContext(context_);
    bindings_system_->UpdateBindingsForContext(context_);
  }
}

ModuleSystemTestEnvironment::~ModuleSystemTestEnvironment() {
  if (context_)
    ShutdownModuleSystem();
}

void ModuleSystemTestEnvironment::RegisterModule(const std::string& name,
                                                 const std::string& code) {
  source_map_->RegisterModule(name, code);
}

void ModuleSystemTestEnvironment::RegisterModule(const std::string& name,
                                                 int resource_id) {
  const std::string& code = ResourceBundle::GetSharedInstance()
                                .GetRawDataResource(resource_id)
                                .as_string();
  source_map_->RegisterModule(name, code);
}

void ModuleSystemTestEnvironment::OverrideNativeHandler(
    const std::string& name,
    const std::string& code) {
  RegisterModule(name, code);
  context_->module_system()->OverrideNativeHandlerForTest(name);
}

void ModuleSystemTestEnvironment::RegisterTestFile(
    const std::string& module_name,
    const std::string& file_name) {
  base::FilePath test_js_file_path;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_js_file_path));
  test_js_file_path = test_js_file_path.AppendASCII(file_name);
  std::string test_js;
  ASSERT_TRUE(base::ReadFileToString(test_js_file_path, &test_js));
  source_map_->RegisterModule(module_name, test_js);
}

void ModuleSystemTestEnvironment::ShutdownGin() {
  context_holder_.reset();
}

void ModuleSystemTestEnvironment::ShutdownModuleSystem() {
  CHECK(context_->is_valid());
  context_->v8_context()->Exit();
  context_set_->Remove(context_);
  base::RunLoop().RunUntilIdle();
  context_ = nullptr;
  assert_natives_ = nullptr;
}

v8::Local<v8::Object> ModuleSystemTestEnvironment::CreateGlobal(
    const std::string& name) {
  v8::EscapableHandleScope handle_scope(isolate_);
  v8::Local<v8::Object> object = v8::Object::New(isolate_);
  isolate_->GetCurrentContext()->Global()->Set(
      v8::String::NewFromUtf8(isolate_, name.c_str()), object);
  return handle_scope.Escape(object);
}

ModuleSystemTest::ModuleSystemTest()
    : isolate_(v8::Isolate::GetCurrent()),
      context_set_(&extension_ids_),
      should_assertions_be_made_(true) {}

ModuleSystemTest::~ModuleSystemTest() {
}

void ModuleSystemTest::SetUp() {
  env_ = CreateEnvironment();
  base::CommandLine::ForCurrentProcess()->AppendSwitch("test-type");
}

void ModuleSystemTest::TearDown() {
  // All tests must assert at least once unless otherwise specified.
  if (env_->assert_natives()) {  // The context may have already been shutdown.
    EXPECT_EQ(should_assertions_be_made_,
              env_->assert_natives()->assertion_made());
    EXPECT_FALSE(env_->assert_natives()->failed());
  } else {
    EXPECT_FALSE(should_assertions_be_made_);
  }
  env_.reset();
  v8::HeapStatistics stats;
  isolate_->GetHeapStatistics(&stats);
  size_t old_heap_size = 0;
  // Run the GC until the heap size reaches a steady state to ensure that
  // all the garbage is collected.
  while (stats.used_heap_size() != old_heap_size) {
    old_heap_size = stats.used_heap_size();
    isolate_->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
    isolate_->GetHeapStatistics(&stats);
  }
}

std::unique_ptr<ModuleSystemTestEnvironment>
ModuleSystemTest::CreateEnvironment() {
  return base::MakeUnique<ModuleSystemTestEnvironment>(isolate_, &context_set_);
}

void ModuleSystemTest::ExpectNoAssertionsMade() {
  should_assertions_be_made_ = false;
}

void ModuleSystemTest::RunResolvedPromises() {
  v8::MicrotasksScope::PerformCheckpoint(isolate_);
}

}  // namespace extensions
