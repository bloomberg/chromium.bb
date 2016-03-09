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

  using PingHandler = Callback<void(const PingCallback&)>;
  void set_ping_handler(const PingHandler& handler) { ping_handler_ = handler; }

  using EchoHandler = Callback<void(int32_t, const EchoCallback&)>;
  void set_echo_handler(const EchoHandler& handler) { echo_handler_ = handler; }

  using AsyncEchoHandler = Callback<void(int32_t, const AsyncEchoCallback&)>;
  void set_async_echo_handler(const AsyncEchoHandler& handler) {
    async_echo_handler_ = handler;
  }

  // TestSync implementation:
  void Ping(const PingCallback& callback) override {
    if (ping_handler_.is_null()) {
      callback.Run();
      return;
    }
    ping_handler_.Run(callback);
  }
  void Echo(int32_t value, const EchoCallback& callback) override {
    if (echo_handler_.is_null()) {
      callback.Run(value);
      return;
    }
    echo_handler_.Run(value, callback);
  }
  void AsyncEcho(int32_t value, const AsyncEchoCallback& callback) override {
    if (async_echo_handler_.is_null()) {
      callback.Run(value);
      return;
    }
    async_echo_handler_.Run(value, callback);
  }

  Binding<TestSync>* binding() { return &binding_; }

 private:
  Binding<TestSync> binding_;
  PingHandler ping_handler_;
  EchoHandler echo_handler_;
  AsyncEchoHandler async_echo_handler_;

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
    impl_->set_ping_handler([this](const TestSync::PingCallback& callback) {
      {
        base::AutoLock locker(lock_);
        ping_called_ = true;
      }
      callback.Run();
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

TEST_F(SyncMethodTest, ReenteredBySyncMethodBinding) {
  // Test that an interface pointer waiting for a sync call response can be
  // reentered by a binding serving sync methods on the same thread.

  TestSyncPtr ptr;
  // The binding lives on the same thread as the interface pointer.
  TestSyncImpl impl(GetProxy(&ptr));
  int32_t output_value = -1;
  ASSERT_TRUE(ptr->Echo(42, &output_value));
  EXPECT_EQ(42, output_value);
}

TEST_F(SyncMethodTest, InterefacePtrDestroyedDuringSyncCall) {
  // Test that it won't result in crash or hang if an interface pointer is
  // destroyed while it is waiting for a sync call response.

  TestSyncPtr ptr;
  TestSyncImpl impl(GetProxy(&ptr));
  impl.set_ping_handler([&ptr](const TestSync::PingCallback& callback) {
    ptr.reset();
    callback.Run();
  });
  ASSERT_FALSE(ptr->Ping());
}

TEST_F(SyncMethodTest, BindingDestroyedDuringSyncCall) {
  // Test that it won't result in crash or hang if a binding is
  // closed (and therefore the message pipe handle is closed) while the
  // corresponding interface pointer is waiting for a sync call response.

  TestSyncPtr ptr;
  TestSyncImpl impl(GetProxy(&ptr));
  impl.set_ping_handler([&impl](const TestSync::PingCallback& callback) {
    impl.binding()->Close();
    callback.Run();
  });
  ASSERT_FALSE(ptr->Ping());
}

TEST_F(SyncMethodTest, NestedSyncCallsWithInOrderResponses) {
  // Test that we can call a sync method on an interface ptr, while there is
  // already a sync call ongoing. The responses arrive in order.

  TestSyncPtr ptr;
  TestSyncImpl impl(GetProxy(&ptr));

  // The same variable is used to store the output of the two sync calls, in
  // order to test that responses are handled in the correct order.
  int32_t result_value = -1;

  bool first_call = true;
  impl.set_echo_handler([&first_call, &ptr, &result_value](
      int32_t value, const TestSync::EchoCallback& callback) {
    if (first_call) {
      first_call = false;
      ASSERT_TRUE(ptr->Echo(456, &result_value));
      EXPECT_EQ(456, result_value);
    }
    callback.Run(value);
  });

  ASSERT_TRUE(ptr->Echo(123, &result_value));
  EXPECT_EQ(123, result_value);
}

TEST_F(SyncMethodTest, NestedSyncCallsWithOutOfOrderResponses) {
  // Test that we can call a sync method on an interface ptr, while there is
  // already a sync call ongoing. The responses arrive out of order.

  TestSyncPtr ptr;
  TestSyncImpl impl(GetProxy(&ptr));

  // The same variable is used to store the output of the two sync calls, in
  // order to test that responses are handled in the correct order.
  int32_t result_value = -1;

  bool first_call = true;
  impl.set_echo_handler([&first_call, &ptr, &result_value](
      int32_t value, const TestSync::EchoCallback& callback) {
    callback.Run(value);
    if (first_call) {
      first_call = false;
      ASSERT_TRUE(ptr->Echo(456, &result_value));
      EXPECT_EQ(456, result_value);
    }
  });

  ASSERT_TRUE(ptr->Echo(123, &result_value));
  EXPECT_EQ(123, result_value);
}

TEST_F(SyncMethodTest, AsyncResponseQueuedDuringSyncCall) {
  // Test that while an interface pointer is waiting for the response to a sync
  // call, async responses are queued until the sync call completes.

  TestSyncPtr ptr;
  TestSyncImpl impl(GetProxy(&ptr));

  int32_t async_echo_request_value = -1;
  TestSync::AsyncEchoCallback async_echo_request_callback;
  base::RunLoop run_loop1;
  impl.set_async_echo_handler(
      [&async_echo_request_value, &async_echo_request_callback, &run_loop1](
          int32_t value, const TestSync::AsyncEchoCallback& callback) {
        async_echo_request_value = value;
        async_echo_request_callback = callback;
        run_loop1.Quit();
      });

  bool async_echo_response_dispatched = false;
  base::RunLoop run_loop2;
  ptr->AsyncEcho(123,
                 [&async_echo_response_dispatched, &run_loop2](int32_t result) {
                   async_echo_response_dispatched = true;
                   EXPECT_EQ(123, result);
                   run_loop2.Quit();
                 });
  // Run until the AsyncEcho request reaches the service side.
  run_loop1.Run();

  impl.set_echo_handler(
      [&async_echo_request_value, &async_echo_request_callback](
          int32_t value, const TestSync::EchoCallback& callback) {
        // Send back the async response first.
        EXPECT_FALSE(async_echo_request_callback.is_null());
        async_echo_request_callback.Run(async_echo_request_value);

        callback.Run(value);
      });

  int32_t result_value = -1;
  ASSERT_TRUE(ptr->Echo(456, &result_value));
  EXPECT_EQ(456, result_value);

  // Although the AsyncEcho response arrives before the Echo response, it should
  // be queued and not yet dispatched.
  EXPECT_FALSE(async_echo_response_dispatched);

  // Run until the AsyncEcho response is dispatched.
  run_loop2.Run();

  EXPECT_TRUE(async_echo_response_dispatched);
}

TEST_F(SyncMethodTest, AsyncRequestQueuedDuringSyncCall) {
  // Test that while an interface pointer is waiting for the response to a sync
  // call, async requests for a binding running on the same thread are queued
  // until the sync call completes.

  TestSyncPtr ptr;
  TestSyncImpl impl(GetProxy(&ptr));

  bool async_echo_request_dispatched = false;
  impl.set_async_echo_handler([&async_echo_request_dispatched](
      int32_t value, const TestSync::AsyncEchoCallback& callback) {
    async_echo_request_dispatched = true;
    callback.Run(value);
  });

  bool async_echo_response_dispatched = false;
  base::RunLoop run_loop;
  ptr->AsyncEcho(123,
                 [&async_echo_response_dispatched, &run_loop](int32_t result) {
                   async_echo_response_dispatched = true;
                   EXPECT_EQ(123, result);
                   run_loop.Quit();
                 });

  impl.set_echo_handler([&async_echo_request_dispatched](
      int32_t value, const TestSync::EchoCallback& callback) {
    // Although the AsyncEcho request is sent before the Echo request, it
    // shouldn't be dispatched yet at this point, because there is an ongoing
    // sync call on the same thread.
    EXPECT_FALSE(async_echo_request_dispatched);
    callback.Run(value);
  });

  int32_t result_value = -1;
  ASSERT_TRUE(ptr->Echo(456, &result_value));
  EXPECT_EQ(456, result_value);

  // Although the AsyncEcho request is sent before the Echo request, it
  // shouldn't be dispatched yet.
  EXPECT_FALSE(async_echo_request_dispatched);

  // Run until the AsyncEcho response is dispatched.
  run_loop.Run();

  EXPECT_TRUE(async_echo_response_dispatched);
}

TEST_F(SyncMethodTest, QueuedMessagesProcessedBeforeErrorNotification) {
  // Test that while an interface pointer is waiting for the response to a sync
  // call, async responses are queued. If the message pipe is disconnected
  // before the queued messages are processed, the connection error
  // notification is delayed until all the queued messages are processed.

  TestSyncPtr ptr;
  TestSyncImpl impl(GetProxy(&ptr));

  int32_t async_echo_request_value = -1;
  TestSync::AsyncEchoCallback async_echo_request_callback;
  base::RunLoop run_loop1;
  impl.set_async_echo_handler(
      [&async_echo_request_value, &async_echo_request_callback, &run_loop1](
          int32_t value, const TestSync::AsyncEchoCallback& callback) {
        async_echo_request_value = value;
        async_echo_request_callback = callback;
        run_loop1.Quit();
      });

  bool async_echo_response_dispatched = false;
  base::RunLoop run_loop2;
  ptr->AsyncEcho(123,
                 [&async_echo_response_dispatched, &run_loop2](int32_t result) {
                   async_echo_response_dispatched = true;
                   EXPECT_EQ(123, result);
                   run_loop2.Quit();
                 });
  // Run until the AsyncEcho request reaches the service side.
  run_loop1.Run();

  impl.set_echo_handler(
      [&impl, &async_echo_request_value, &async_echo_request_callback](
          int32_t value, const TestSync::EchoCallback& callback) {
        // Send back the async response first.
        EXPECT_FALSE(async_echo_request_callback.is_null());
        async_echo_request_callback.Run(async_echo_request_value);

        impl.binding()->Close();
      });

  bool connection_error_dispatched = false;
  base::RunLoop run_loop3;
  ptr.set_connection_error_handler(
      [&connection_error_dispatched, &run_loop3]() {
        connection_error_dispatched = true;
        run_loop3.Quit();
      });

  int32_t result_value = -1;
  ASSERT_FALSE(ptr->Echo(456, &result_value));
  EXPECT_EQ(-1, result_value);
  ASSERT_FALSE(connection_error_dispatched);
  EXPECT_FALSE(ptr.encountered_error());

  // Although the AsyncEcho response arrives before the Echo response, it should
  // be queued and not yet dispatched.
  EXPECT_FALSE(async_echo_response_dispatched);

  // Run until the AsyncEcho response is dispatched.
  run_loop2.Run();

  EXPECT_TRUE(async_echo_response_dispatched);
  ASSERT_FALSE(connection_error_dispatched);
  EXPECT_FALSE(ptr.encountered_error());

  // Run until the error notification is dispatched.
  run_loop3.Run();

  ASSERT_TRUE(connection_error_dispatched);
  EXPECT_TRUE(ptr.encountered_error());
}

TEST_F(SyncMethodTest, InvalidMessageDuringSyncCall) {
  // Test that while an interface pointer is waiting for the response to a sync
  // call, an invalid incoming message will disconnect the message pipe, cause
  // the sync call to return false, and run the connection error handler
  // asynchronously.

  MessagePipe pipe;

  TestSyncPtr ptr;
  ptr.Bind(TestSyncPtrInfo(std::move(pipe.handle0), 0u));

  MessagePipeHandle raw_binding_handle = pipe.handle1.get();
  TestSyncImpl impl(MakeRequest<TestSync>(std::move(pipe.handle1)));

  impl.set_echo_handler([&raw_binding_handle](
      int32_t value, const TestSync::EchoCallback& callback) {
    // Write a 1-byte message, which is considered invalid.
    char invalid_message = 0;
    MojoResult result =
        WriteMessageRaw(raw_binding_handle, &invalid_message, 1u, nullptr, 0u,
                        MOJO_WRITE_MESSAGE_FLAG_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    callback.Run(value);
  });

  bool connection_error_dispatched = false;
  base::RunLoop run_loop;
  ptr.set_connection_error_handler([&connection_error_dispatched, &run_loop]() {
    connection_error_dispatched = true;
    run_loop.Quit();
  });

  int32_t result_value = -1;
  ASSERT_FALSE(ptr->Echo(456, &result_value));
  EXPECT_EQ(-1, result_value);
  ASSERT_FALSE(connection_error_dispatched);

  run_loop.Run();
  ASSERT_TRUE(connection_error_dispatched);
}

}  // namespace
}  // namespace test
}  // namespace mojo
