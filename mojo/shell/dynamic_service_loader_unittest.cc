// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "mojo/shell/context.h"
#include "mojo/shell/dynamic_service_loader.h"
#include "mojo/shell/dynamic_service_runner.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {

namespace {

struct TestState {
  TestState() : runner_was_started(false), runner_was_destroyed(false) {}
  bool runner_was_started;
  bool runner_was_destroyed;
};

class TestDynamicServiceRunner : public DynamicServiceRunner {
 public:
  explicit TestDynamicServiceRunner(TestState* state) : state_(state) {}
  virtual ~TestDynamicServiceRunner() {
    state_->runner_was_destroyed = true;
    base::MessageLoop::current()->Quit();
  }
  virtual void Start(const base::FilePath& app_path,
                     ScopedMessagePipeHandle service_handle,
                     const base::Closure& app_completed_callback) OVERRIDE {
    state_->runner_was_started = true;
  }
 private:
  TestState* state_;
};

class TestDynamicServiceRunnerFactory : public DynamicServiceRunnerFactory {
 public:
  explicit TestDynamicServiceRunnerFactory(TestState* state) : state_(state) {}
  virtual ~TestDynamicServiceRunnerFactory() {}
  virtual scoped_ptr<DynamicServiceRunner> Create(Context* context) OVERRIDE {
    return scoped_ptr<DynamicServiceRunner>(
        new TestDynamicServiceRunner(state_));
  }
 private:
  TestState* state_;
};

}  // namespace

class DynamicServiceLoaderTest : public testing::Test {
 public:
  DynamicServiceLoaderTest() {}
  virtual ~DynamicServiceLoaderTest() {}
  virtual void SetUp() OVERRIDE {
    context_.Init();
    scoped_ptr<DynamicServiceRunnerFactory> factory(
        new TestDynamicServiceRunnerFactory(&state_));
    loader_.reset(new DynamicServiceLoader(&context_, factory.Pass()));
  }
 protected:
  Context context_;
  base::MessageLoop loop_;
  scoped_ptr<DynamicServiceLoader> loader_;
  TestState state_;
};

TEST_F(DynamicServiceLoaderTest, DoesNotExist) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath nonexistent_file(FILE_PATH_LITERAL("nonexistent.txt"));
  GURL url(net::FilePathToFileURL(temp_dir.path().Append(nonexistent_file)));
  MessagePipe pipe;
  loader_->LoadService(context_.service_manager(), url, pipe.handle0.Pass());
  loop_.Run();
  EXPECT_FALSE(state_.runner_was_started);
  EXPECT_TRUE(state_.runner_was_destroyed);
}

}  // namespace shell
}  // namespace mojo
