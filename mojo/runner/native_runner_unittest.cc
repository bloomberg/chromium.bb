// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "mojo/package_manager/package_manager_impl.h"
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
    loop_.reset(new base::MessageLoop);
  }
  ~NativeApplicationLoaderTest() override {
    loop_.reset();
  }
  void SetUp() override {
    base::FilePath shell_dir;
    PathService::Get(base::DIR_MODULE, &shell_dir);
    scoped_ptr<package_manager::PackageManagerImpl> package_manager(
        new package_manager::PackageManagerImpl(shell_dir, nullptr));
    scoped_ptr<shell::NativeRunnerFactory> factory(
        new TestNativeRunnerFactory(&state_));
    application_manager_.reset(new shell::ApplicationManager(
        package_manager.Pass(), factory.Pass(), nullptr));
  }
  void TearDown() override { application_manager_.reset(); }

 protected:
  scoped_ptr<base::MessageLoop> loop_;
  scoped_ptr<shell::ApplicationManager> application_manager_;
  TestState state_;
};

TEST_F(NativeApplicationLoaderTest, DoesNotExist) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath nonexistent_file(FILE_PATH_LITERAL("nonexistent.txt"));
  GURL url(util::FilePathToFileURL(temp_dir.path().Append(nonexistent_file)));

  scoped_ptr<shell::ConnectToApplicationParams> params(
      new shell::ConnectToApplicationParams);
  params->SetTargetURL(url);
  application_manager_->ConnectToApplication(params.Pass());
  EXPECT_FALSE(state_.runner_was_created);
  EXPECT_FALSE(state_.runner_was_started);
  EXPECT_FALSE(state_.runner_was_destroyed);
}

}  // namespace
}  // namespace runner
}  // namespace mojo
