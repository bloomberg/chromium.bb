// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "mojo/shell/context.h"
#include "mojo/shell/dynamic_application_loader.h"
#include "mojo/shell/dynamic_service_runner.h"
#include "mojo/shell/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {

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

class TestDynamicServiceRunner : public DynamicServiceRunner {
 public:
  explicit TestDynamicServiceRunner(TestState* state) : state_(state) {
    state_->runner_was_created = true;
  }
  ~TestDynamicServiceRunner() override {
    state_->runner_was_destroyed = true;
    base::MessageLoop::current()->Quit();
  }
  void Start(const base::FilePath& app_path,
             ScopedMessagePipeHandle service_handle,
             const base::Closure& app_completed_callback) override {
    state_->runner_was_started = true;
  }

 private:
  TestState* state_;
};

class TestDynamicServiceRunnerFactory : public DynamicServiceRunnerFactory {
 public:
  explicit TestDynamicServiceRunnerFactory(TestState* state) : state_(state) {}
  ~TestDynamicServiceRunnerFactory() override {}
  scoped_ptr<DynamicServiceRunner> Create(Context* context) override {
    return scoped_ptr<DynamicServiceRunner>(
        new TestDynamicServiceRunner(state_));
  }

 private:
  TestState* state_;
};

}  // namespace

class DynamicApplicationLoaderTest : public testing::Test {
 public:
  DynamicApplicationLoaderTest() {}
  virtual ~DynamicApplicationLoaderTest() {}
  virtual void SetUp() override {
    context_.Init();
    scoped_ptr<DynamicServiceRunnerFactory> factory(
        new TestDynamicServiceRunnerFactory(&state_));
    loader_.reset(new DynamicApplicationLoader(&context_, factory.Pass()));
  }

 protected:
  Context context_;
  base::MessageLoop loop_;
  scoped_ptr<DynamicApplicationLoader> loader_;
  TestState state_;
};

TEST_F(DynamicApplicationLoaderTest, DoesNotExist) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath nonexistent_file(FILE_PATH_LITERAL("nonexistent.txt"));
  GURL url(FilePathToFileURL(temp_dir.path().Append(nonexistent_file)));
  MessagePipe pipe;
  scoped_refptr<ApplicationLoader::SimpleLoadCallbacks> callbacks(
      new ApplicationLoader::SimpleLoadCallbacks(pipe.handle0.Pass()));
  loader_->Load(context_.application_manager(), url, callbacks);
  EXPECT_FALSE(state_.runner_was_created);
  EXPECT_FALSE(state_.runner_was_started);
  EXPECT_FALSE(state_.runner_was_destroyed);
}

}  // namespace shell
}  // namespace mojo
