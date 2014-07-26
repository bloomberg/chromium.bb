// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_test_base.h"

#include <vector>

#include "base/run_loop.h"
#include "extensions/common/extension_urls.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/process_info_native_handler.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/bindings/js/core.h"
#include "mojo/bindings/js/handle.h"
#include "mojo/bindings/js/support.h"
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

  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE {
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
    base::MessageLoop::current()->PostTask(FROM_HERE, quit_closure_);
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

gin::WrapperInfo TestServiceProvider::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::Handle<TestServiceProvider> TestServiceProvider::Create(
    v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, new TestServiceProvider());
}

TestServiceProvider::~TestServiceProvider() {
}

gin::ObjectTemplateBuilder TestServiceProvider::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<TestServiceProvider>::GetObjectTemplateBuilder(isolate)
      .SetMethod("connectToService", &TestServiceProvider::ConnectToService);
}

mojo::Handle TestServiceProvider::ConnectToService(
    const std::string& service_name) {
  EXPECT_EQ(1u, service_factories_.count(service_name))
      << "Unregistered service " << service_name << " requested.";
  mojo::MessagePipe pipe;
  std::map<std::string,
           base::Callback<void(mojo::ScopedMessagePipeHandle)> >::iterator it =
      service_factories_.find(service_name);
  if (it != service_factories_.end())
    it->second.Run(pipe.handle0.Pass());
  return pipe.handle1.release();
}

TestServiceProvider::TestServiceProvider() {
}

ApiTestBase::ApiTestBase() {
}
ApiTestBase::~ApiTestBase() {
}

void ApiTestBase::SetUp() {
  ModuleSystemTest::SetUp();
  InitializeEnvironment();
  RegisterModules();
}

void ApiTestBase::RegisterModules() {
  v8_schema_registry_.reset(new V8SchemaRegistry);
  const std::vector<std::pair<std::string, int> > resources =
      Dispatcher::GetJsResources();
  for (std::vector<std::pair<std::string, int> >::const_iterator resource =
           resources.begin();
       resource != resources.end();
       ++resource) {
    if (resource->first != "test_environment_specific_bindings")
      env()->RegisterModule(resource->first, resource->second);
  }
  Dispatcher::RegisterNativeHandlers(env()->module_system(),
                                     env()->context(),
                                     NULL,
                                     NULL,
                                     v8_schema_registry_.get());
  env()->module_system()->RegisterNativeHandler(
      "process",
      scoped_ptr<NativeHandler>(new ProcessInfoNativeHandler(
          env()->context(),
          env()->context()->GetExtensionID(),
          env()->context()->GetContextTypeDescription(),
          false,
          2,
          false)));
  env()->RegisterTestFile("test_environment_specific_bindings",
                          "unit_test_environment_specific_bindings.js");

  env()->OverrideNativeHandler("activityLogger",
                               "exports.LogAPICall = function() {};");
  env()->OverrideNativeHandler(
      "apiDefinitions",
      "exports.GetExtensionAPIDefinitionsForTest = function() { return [] };");
  env()->OverrideNativeHandler(
      "event_natives",
      "exports.AttachEvent = function() {};"
      "exports.DetachEvent = function() {};"
      "exports.AttachFilteredEvent = function() {};"
      "exports.AttachFilteredEvent = function() {};"
      "exports.MatchAgainstEventFilter = function() { return [] };");

  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(env()->isolate(),
                         mojo::js::Core::kModuleName,
                         mojo::js::Core::GetModule(env()->isolate()));
  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(env()->isolate(),
                         mojo::js::Support::kModuleName,
                         mojo::js::Support::GetModule(env()->isolate()));
  gin::Handle<TestServiceProvider> service_provider =
      TestServiceProvider::Create(env()->isolate());
  service_provider_ = service_provider.get();
  gin::ModuleRegistry::From(env()->context()->v8_context())
      ->AddBuiltinModule(env()->isolate(),
                         "content/public/renderer/service_provider",
                         service_provider.ToV8());
}

void ApiTestBase::InitializeEnvironment() {
  gin::Dictionary global(env()->isolate(),
                         env()->context()->v8_context()->Global());
  gin::Dictionary navigator(gin::Dictionary::CreateEmpty(env()->isolate()));
  navigator.Set("appVersion", base::StringPiece(""));
  global.Set("navigator", navigator);
  gin::Dictionary chrome(gin::Dictionary::CreateEmpty(env()->isolate()));
  global.Set("chrome", chrome);
  gin::Dictionary extension(gin::Dictionary::CreateEmpty(env()->isolate()));
  chrome.Set("extension", extension);
  gin::Dictionary runtime(gin::Dictionary::CreateEmpty(env()->isolate()));
  chrome.Set("runtime", runtime);
}

void ApiTestBase::RunTest(const std::string& file_name,
                          const std::string& test_name) {
  env()->RegisterTestFile("testBody", file_name);
  ExpectNoAssertionsMade();
  base::RunLoop run_loop;
  gin::ModuleRegistry::From(env()->context()->v8_context())->AddBuiltinModule(
      env()->isolate(),
      "testNatives",
      TestNatives::Create(env()->isolate(), run_loop.QuitClosure()).ToV8());
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(&ApiTestBase::RunTestInner,
                                                    base::Unretained(this),
                                                    test_name,
                                                    run_loop.QuitClosure()));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ApiTestBase::RunPromisesAgain, base::Unretained(this)));
  run_loop.Run();
}

void ApiTestBase::RunTestInner(const std::string& test_name,
                               const base::Closure& quit_closure) {
  v8::HandleScope scope(env()->isolate());
  ModuleSystem::NativesEnabledScope natives_enabled(env()->module_system());
  v8::Handle<v8::Value> result =
      env()->module_system()->CallModuleMethod("testBody", test_name);
  if (!result->IsTrue()) {
    base::MessageLoop::current()->PostTask(FROM_HERE, quit_closure);
    FAIL() << "Failed to run test \"" << test_name << "\"";
  }
}

void ApiTestBase::RunPromisesAgain() {
  RunResolvedPromises();
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ApiTestBase::RunPromisesAgain, base::Unretained(this)));
}

}  // namespace extensions
