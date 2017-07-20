// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_test_base.h"

#include <utility>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/process_info_native_handler.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/modules/console.h"
#include "gin/modules/timer.h"
#include "mojo/edk/js/core.h"
#include "mojo/edk/js/handle.h"
#include "mojo/edk/js/support.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/core.h"

namespace extensions {
namespace {

// Natives for the implementation of the unit test version of chrome.test. Calls
// the provided |quit_closure| when either notifyPass or notifyFail is called.
class TestNatives : public gin::Wrappable<TestNatives> {
 public:
  static gin::Handle<TestNatives> Create(v8::Isolate* isolate,
                                         const base::Closure& quit_closure) {
    return gin::CreateHandle(isolate, new TestNatives(quit_closure));
  }

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return Wrappable<TestNatives>::GetObjectTemplateBuilder(isolate)
        .SetMethod("Log", &TestNatives::Log)
        .SetMethod("NotifyPass", &TestNatives::NotifyPass)
        .SetMethod("NotifyFail", &TestNatives::NotifyFail);
  }

  void Log(const std::string& value) { logs_ += value + "\n"; }
  void NotifyPass() { FinishTesting(); }

  void NotifyFail(const std::string& message) {
    FinishTesting();
    FAIL() << logs_ << message;
  }

  void FinishTesting() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  }

  static gin::WrapperInfo kWrapperInfo;

 private:
  explicit TestNatives(const base::Closure& quit_closure)
      : quit_closure_(quit_closure) {}

  const base::Closure quit_closure_;
  std::string logs_;
};

gin::WrapperInfo TestNatives::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace

gin::WrapperInfo TestInterfaceProvider::kWrapperInfo =
    {gin::kEmbedderNativeGin};

gin::Handle<TestInterfaceProvider> TestInterfaceProvider::Create(
    v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, new TestInterfaceProvider());
}

TestInterfaceProvider::~TestInterfaceProvider() {
}

gin::ObjectTemplateBuilder TestInterfaceProvider::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<TestInterfaceProvider>::GetObjectTemplateBuilder(isolate)
      .SetMethod("getInterface", &TestInterfaceProvider::GetInterface);
}

mojo::Handle TestInterfaceProvider::GetInterface(
    const std::string& interface_name) {
  EXPECT_EQ(1u, factories_.count(interface_name))
      << "Unregistered interface " << interface_name << " requested.";
  mojo::MessagePipe pipe;
  std::map<std::string,
           base::Callback<void(mojo::ScopedMessagePipeHandle)> >::iterator it =
      factories_.find(interface_name);
  if (it != factories_.end())
    it->second.Run(std::move(pipe.handle0));
  return pipe.handle1.release();
}

TestInterfaceProvider::TestInterfaceProvider() {
}

// static
void TestInterfaceProvider::IgnoreHandle(mojo::ScopedMessagePipeHandle handle) {
}

ApiTestEnvironment::ApiTestEnvironment(
    ModuleSystemTestEnvironment* environment) {
  env_ = environment;
  InitializeEnvironment();
  RegisterModules();
}

ApiTestEnvironment::~ApiTestEnvironment() {
}

void ApiTestEnvironment::RegisterModules() {
  v8_schema_registry_.reset(new V8SchemaRegistry);
  const std::vector<std::pair<const char*, int>> resources =
      Dispatcher::GetJsResources();
  for (const auto& resource : resources) {
    if (base::StringPiece(resource.first) !=
        "test_environment_specific_bindings")
      env()->RegisterModule(resource.first, resource.second);
  }
  Dispatcher::RegisterNativeHandlers(env()->module_system(),
                                     env()->context(),
                                     NULL,
                                     NULL,
                                     v8_schema_registry_.get());
  env()->module_system()->RegisterNativeHandler(
      "process", std::unique_ptr<NativeHandler>(new ProcessInfoNativeHandler(
                     env()->context(), env()->context()->GetExtensionID(),
                     env()->context()->GetContextTypeDescription(), false,
                     false, 2, false)));
  env()->RegisterTestFile("test_environment_specific_bindings",
                          "unit_test_environment_specific_bindings.js");

  env()->OverrideNativeHandler("activityLogger",
                               "exports.$set('LogAPICall', function() {});");
  env()->OverrideNativeHandler(
      "apiDefinitions",
      "exports.$set('GetExtensionAPIDefinitionsForTest',"
                    "function() { return [] });");
  env()->OverrideNativeHandler(
      "event_natives",
      "exports.$set('AttachEvent', function() {});"
      "exports.$set('DetachEvent', function() {});"
      "exports.$set('AttachFilteredEvent', function() {});"
      "exports.$set('AttachFilteredEvent', function() {});"
      "exports.$set('MatchAgainstEventFilter', function() { return [] });");

  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(env()->isolate(), gin::Console::kModuleName,
                         gin::Console::GetModule(env()->isolate()));
  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(env()->isolate(), gin::TimerModule::kName,
                         gin::TimerModule::GetModule(env()->isolate()));
  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(env()->isolate(), mojo::edk::js::Core::kModuleName,
                         mojo::edk::js::Core::GetModule(env()->isolate()));
  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(env()->isolate(), mojo::edk::js::Support::kModuleName,
                         mojo::edk::js::Support::GetModule(env()->isolate()));
  gin::Handle<TestInterfaceProvider> interface_provider =
    TestInterfaceProvider::Create(env()->isolate());
  interface_provider_ = interface_provider.get();
  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(env()->isolate(),
                         "content/public/renderer/frame_interfaces",
                         interface_provider.ToV8());
}

void ApiTestEnvironment::InitializeEnvironment() {
  // With native bindings, we use the actual bindings system to set up the
  // context, so there's no need to provide these stubs.
  if (FeatureSwitch::native_crx_bindings()->IsEnabled())
    return;

  gin::Dictionary global(env()->isolate(),
                         env()->context()->v8_context()->Global());
  gin::Dictionary navigator(gin::Dictionary::CreateEmpty(env()->isolate()));
  navigator.Set("appVersion", base::StringPiece(""));
  global.Set("navigator", navigator);
  gin::Dictionary chrome(gin::Dictionary::CreateEmpty(env()->isolate()));
  global.Set("chrome", chrome);
  gin::Dictionary runtime(gin::Dictionary::CreateEmpty(env()->isolate()));
  chrome.Set("runtime", runtime);
}

void ApiTestEnvironment::RunTest(const std::string& file_name,
                                 const std::string& test_name) {
  env()->RegisterTestFile("testBody", file_name);
  base::RunLoop run_loop;
  gin::ModuleRegistry::From(env()->context()->v8_context())->AddBuiltinModule(
      env()->isolate(),
      "testNatives",
      TestNatives::Create(env()->isolate(), run_loop.QuitClosure()).ToV8());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ApiTestEnvironment::RunTestInner, base::Unretained(this),
                 test_name, run_loop.QuitClosure()));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ApiTestEnvironment::RunPromisesAgain,
                            base::Unretained(this)));
  run_loop.Run();
}

void ApiTestEnvironment::RunTestInner(const std::string& test_name,
                                      const base::Closure& quit_closure) {
  v8::HandleScope scope(env()->isolate());
  ModuleSystem::NativesEnabledScope natives_enabled(env()->module_system());
  v8::Local<v8::Value> result;
  bool did_run = false;
  auto callback = [](bool* did_run, const base::Closure& quit_closure,
                     const std::string& test_name,
                     const std::vector<v8::Local<v8::Value>>& result) {
    *did_run = true;
    if (result.empty() || result[0].IsEmpty() || !result[0]->IsTrue()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure);
      FAIL() << "Failed to run test \"" << test_name << "\"";
    }
  };

  ASSERT_FALSE(
      env()->module_system()->Require("testBody").ToLocalChecked().IsEmpty());
  env()->module_system()->CallModuleMethodSafe(
      "testBody", test_name, 0, nullptr,
      base::Bind(callback, &did_run, quit_closure, test_name));
  ASSERT_TRUE(did_run);
}

void ApiTestEnvironment::RunPromisesAgain() {
  v8::MicrotasksScope::PerformCheckpoint(env()->isolate());
}

ApiTestBase::ApiTestBase() {
}

ApiTestBase::~ApiTestBase() {
}

void ApiTestBase::SetUp() {
  ModuleSystemTest::SetUp();
  test_env_.reset(new ApiTestEnvironment(env()));
}

void ApiTestBase::RunTest(const std::string& file_name,
                          const std::string& test_name) {
  ExpectNoAssertionsMade();
  test_env_->RunTest(file_name, test_name);
}

}  // namespace extensions
