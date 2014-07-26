// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_TEST_BASE_H_
#define EXTENSIONS_RENDERER_API_TEST_BASE_H_

#include <map>
#include <string>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "extensions/renderer/module_system_test.h"
#include "extensions/renderer/v8_schema_registry.h"
#include "gin/handle.h"
#include "gin/modules/module_registry.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/bindings/js/handle.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/core.h"

namespace extensions {

class V8SchemaRegistry;

// A ServiceProvider that provides access from JS modules to services registered
// by AddService() calls.
class TestServiceProvider : public gin::Wrappable<TestServiceProvider> {
 public:
  static gin::Handle<TestServiceProvider> Create(v8::Isolate* isolate);
  virtual ~TestServiceProvider();

  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  template <typename Interface>
  void AddService(const base::Callback<void(mojo::InterfaceRequest<Interface>)>
                      service_factory) {
    service_factories_.insert(std::make_pair(
        Interface::Name_,
        base::Bind(ForwardToServiceFactory<Interface>, service_factory)));
  }

  static gin::WrapperInfo kWrapperInfo;

 private:
  TestServiceProvider();

  mojo::Handle ConnectToService(const std::string& service_name);

  template <typename Interface>
  static void ForwardToServiceFactory(
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>
          service_factory,
      mojo::ScopedMessagePipeHandle handle) {
    service_factory.Run(mojo::MakeRequest<Interface>(handle.Pass()));
  }
  std::map<std::string, base::Callback<void(mojo::ScopedMessagePipeHandle)> >
      service_factories_;
};

// A base class for unit testing apps/extensions API custom bindings implemented
// on Mojo services. To use:
// 1. Register test Mojo service implementations on service_provider().
// 2. Write JS tests in extensions/test/data/test_file.js.
// 3. Write one C++ test function for each JS test containing
//    RunTest("test_file.js", "testFunctionName").
// See extensions/renderer/api_test_base_unittest.cc and
// extensions/test/data/api_test_base_unittest.js for sample usage.
class ApiTestBase : public ModuleSystemTest {
 protected:
  ApiTestBase();
  virtual ~ApiTestBase();
  virtual void SetUp() OVERRIDE;
  void RunTest(const std::string& file_name, const std::string& test_name);
  TestServiceProvider* service_provider() { return service_provider_; }

 private:
  void RegisterModules();
  void InitializeEnvironment();
  void RunTestInner(const std::string& test_name,
                    const base::Closure& quit_closure);
  void RunPromisesAgain();

  base::MessageLoop message_loop_;
  TestServiceProvider* service_provider_;
  scoped_ptr<V8SchemaRegistry> v8_schema_registry_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_TEST_BASE_H_
