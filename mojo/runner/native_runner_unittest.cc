// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "mojo/runner/context.h"
#include "mojo/runner/url_resolver.h"
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
             shell::NativeApplicationCleanup cleanup,
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
  scoped_ptr<shell::NativeRunner> Create(const Options& options) override {
    return scoped_ptr<shell::NativeRunner>(new TestNativeRunner(state_));
  }

 private:
  TestState* state_;
};

class NativeApplicationLoaderTest : public testing::Test,
                                    public shell::ApplicationManager::Delegate {
 public:
  NativeApplicationLoaderTest() : application_manager_(this) {}
  ~NativeApplicationLoaderTest() override {}
  void SetUp() override {
    context_.Init();
    scoped_ptr<shell::NativeRunnerFactory> factory(
        new TestNativeRunnerFactory(&state_));
    application_manager_.set_native_runner_factory(factory.Pass());
    application_manager_.set_blocking_pool(
        context_.task_runners()->blocking_pool());
  }
  void TearDown() override { context_.Shutdown(); }

 protected:
  Context context_;
  base::MessageLoop loop_;
  shell::ApplicationManager application_manager_;
  TestState state_;

 private:
  // shell::ApplicationManager::Delegate
  GURL ResolveMappings(const GURL& url) override {
    return context_.url_resolver()->ApplyMappings(url);
  }
  GURL ResolveMojoURL(const GURL& url) override {
    return context_.url_resolver()->ResolveMojoURL(url);
  }
  bool CreateFetcher(
      const GURL& url,
      const shell::Fetcher::FetchCallback& loader_callback) override {
    return false;
  }
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
  application_manager_.ConnectToApplication(
      nullptr, request.Pass(), std::string(), GURL(), services.Pass(),
      service_provider.Pass(), shell::GetPermissiveCapabilityFilter(),
      base::Closure());
  EXPECT_FALSE(state_.runner_was_created);
  EXPECT_FALSE(state_.runner_was_started);
  EXPECT_FALSE(state_.runner_was_destroyed);
}

}  // namespace
}  // namespace runner
}  // namespace mojo
