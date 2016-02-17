// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/interfaces/bindings/tests/test_sync_methods.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class SyncMethodTest : public testing::Test {
 public:
  SyncMethodTest() : loop_(common::MessagePumpMojo::Create()) {}
  ~SyncMethodTest() override { loop_.RunUntilIdle(); }

 private:
  base::MessageLoop loop_;
};

class TestSyncImpl : public TestSync {
 public:
  TestSyncImpl(TestSyncRequest request) : binding_(this, std::move(request)) {}

  void set_ping_notification(const Closure& closure) {
    ping_notification_ = closure;
  }

  // TestSync implementation:
  void Get(const GetCallback& callback) override { callback.Run(42); }
  void Set(int32_t value, const SetCallback& callback) override {
    callback.Run();
  }
  void Ping(const PingCallback& callback) override {
    ping_notification_.Run();
    callback.Run();
  }
  void Echo(int32_t value, const EchoCallback& callback) override {
    callback.Run(value);
  }

 private:
  Binding<TestSync> binding_;
  Closure ping_notification_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncImpl);
};

class TestSyncServiceThread {
 public:
  TestSyncServiceThread()
      : thread_("TestSyncServiceThread"), ping_called_(false) {
    base::Thread::Options thread_options;
    thread_options.message_pump_factory =
        base::Bind(&common::MessagePumpMojo::Create);
    thread_.StartWithOptions(thread_options);
  }

  void SetUp(TestSyncRequest request) {
    CHECK(thread_.task_runner()->BelongsToCurrentThread());
    impl_.reset(new TestSyncImpl(std::move(request)));
    impl_->set_ping_notification([this]() {
      base::AutoLock locker(lock_);
      ping_called_ = true;
    });
  }

  void TearDown() {
    CHECK(thread_.task_runner()->BelongsToCurrentThread());
    impl_.reset();
  }

  base::Thread* thread() { return &thread_; }
  bool ping_called() const {
    base::AutoLock locker(lock_);
    return ping_called_;
  }

 private:
  base::Thread thread_;

  scoped_ptr<TestSyncImpl> impl_;

  mutable base::Lock lock_;
  bool ping_called_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncServiceThread);
};

TEST_F(SyncMethodTest, CallSyncMethodAsynchronously) {
  TestSyncPtr ptr;
  TestSyncImpl impl(GetProxy(&ptr));

  base::RunLoop run_loop;
  ptr->Echo(123, [&run_loop](int32_t result) {
    EXPECT_EQ(123, result);
    run_loop.Quit();
  });
  run_loop.Run();
}

TEST_F(SyncMethodTest, BasicSyncCalls) {
  TestSyncPtr ptr;

  TestSyncServiceThread service_thread;
  service_thread.thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&TestSyncServiceThread::SetUp,
                            base::Unretained(&service_thread),
                            base::Passed(GetProxy(&ptr))));
  ASSERT_TRUE(ptr->Ping());
  ASSERT_TRUE(service_thread.ping_called());

  int32_t output_value = -1;
  ASSERT_TRUE(ptr->Echo(42, &output_value));
  ASSERT_EQ(42, output_value);

  base::RunLoop run_loop;
  service_thread.thread()->task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&TestSyncServiceThread::TearDown,
                            base::Unretained(&service_thread)),
      run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace
}  // namespace test
}  // namespace mojo
