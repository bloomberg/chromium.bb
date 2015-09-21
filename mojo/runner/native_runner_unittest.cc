// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "mojo/runner/context.h"
#include "mojo/shell/application_manager.h"
#include "mojo/util/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace runner {
namespace {

struct TestState {
  TestState()
      : runner_was_created(false),
        runner_was_started(false),
        runner_was_destroyed(false) {}

  bool runner_was_created;
  bool runner_was_started;
  bool runner_was_destroyed;
};

class TestNativeRunner : public shell::NativeRunner {
 public:
  explicit TestNativeRunner(TestState* state) : state_(state) {
    state_->runner_was_created = true;
  }
  ~TestNativeRunner() override {
    state_->runner_was_destroyed = true;
    if (base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->Quit();
  }
  void Start(const base::FilePath& app_path,
             bool start_sandboxed,
             InterfaceRequest<Application> application_request,
             const base::Closure& app_completed_callback) override {
    state_->runner_was_started = true;
  }

 private:
  TestState* state_;
};

class TestNativeRunnerFactory : public shell::NativeRunnerFactory {
 public:
  explicit TestNativeRunnerFactory(TestState* state) : state_(state) {}
  ~TestNativeRunnerFactory() override {}
  scoped_ptr<shell::NativeRunner> Create() override {
    return scoped_ptr<shell::NativeRunner>(new TestNativeRunner(state_));
  }

 private:
  TestState* state_;
};

class NativeApplicationLoaderTest : public testing::Test {
 public:
  NativeApplicationLoaderTest() {
    base::FilePath shell_dir;
    PathService::Get(base::DIR_MODULE, &shell_dir);
    context_.reset(new Context(shell_dir));
    loop_.reset(new base::MessageLoop);
  }
  ~NativeApplicationLoaderTest() override {
    loop_.reset();
    context_.reset();
  }
  void SetUp() override {
    context_->Init();
    scoped_ptr<shell::NativeRunnerFactory> factory(
        new TestNativeRunnerFactory(&state_));
    context_->application_manager()->set_native_runner_factory(factory.Pass());
    context_->application_manager()->set_blocking_pool(
        context_->task_runners()->blocking_pool());
  }
  void TearDown() override { context_->Shutdown(); }

 protected:
  scoped_ptr<base::MessageLoop> loop_;
  scoped_ptr<Context> context_;
  TestState state_;
};

TEST_F(NativeApplicationLoaderTest, DoesNotExist) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath nonexistent_file(FILE_PATH_LITERAL("nonexistent.txt"));
  GURL url(util::FilePathToFileURL(temp_dir.path().Append(nonexistent_file)));
  InterfaceRequest<ServiceProvider> services;
  ServiceProviderPtr service_provider;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(url.spec());

  scoped_ptr<shell::ConnectToApplicationParams> params(
      new shell::ConnectToApplicationParams);
  params->SetURLInfo(request.Pass());
  params->set_services(services.Pass());
  params->set_exposed_services(service_provider.Pass());
  params->set_filter(shell::GetPermissiveCapabilityFilter());
  context_->application_manager()->ConnectToApplication(params.Pass());
  EXPECT_FALSE(state_.runner_was_created);
  EXPECT_FALSE(state_.runner_was_started);
  EXPECT_FALSE(state_.runner_was_destroyed);
}

}  // namespace
}  // namespace runner
}  // namespace mojo
