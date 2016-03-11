// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This file tests both binding.h (mojo::Binding) and strong_binding.h
// (mojo::StrongBinding).

#include "mojo/public/cpp/bindings/binding.h"

#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class BindingTestBase : public testing::Test {
 public:
  BindingTestBase() : loop_(common::MessagePumpMojo::Create()) {}
  ~BindingTestBase() override {}

  base::MessageLoop& loop() { return loop_; }

 private:
  base::MessageLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(BindingTestBase);
};

class ServiceImpl : public sample::Service {
 public:
  explicit ServiceImpl(bool* was_deleted = nullptr)
      : was_deleted_(was_deleted) {}
  ~ServiceImpl() override {
    if (was_deleted_)
      *was_deleted_ = true;
  }

 private:
  // sample::Service implementation
  void Frobinate(sample::FooPtr foo,
                 BazOptions options,
                 sample::PortPtr port,
                 const FrobinateCallback& callback) override {
    callback.Run(1);
  }
  void GetPort(InterfaceRequest<sample::Port> port) override {}

  bool* const was_deleted_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

// BindingTest -----------------------------------------------------------------

using BindingTest = BindingTestBase;

TEST_F(BindingTest, Close) {
  bool called = false;
  sample::ServicePtr ptr;
  auto request = GetProxy(&ptr);
  base::RunLoop run_loop;
  ptr.set_connection_error_handler([&called, &run_loop]() {
    called = true;
    run_loop.Quit();
  });
  ServiceImpl impl;
  Binding<sample::Service> binding(&impl, std::move(request));

  binding.Close();
  EXPECT_FALSE(called);
  run_loop.Run();
  EXPECT_TRUE(called);
}

// Tests that destroying a mojo::Binding closes the bound message pipe handle.
TEST_F(BindingTest, DestroyClosesMessagePipe) {
  bool encountered_error = false;
  ServiceImpl impl;
  sample::ServicePtr ptr;
  auto request = GetProxy(&ptr);
  base::RunLoop run_loop;
  ptr.set_connection_error_handler(
      [&encountered_error, &run_loop]() {
        encountered_error = true;
        run_loop.Quit();
      });
  bool called = false;
  base::RunLoop run_loop2;
  auto called_cb = [&called, &run_loop2](int32_t result) {
    called = true;
    run_loop2.Quit();
  };
  {
    Binding<sample::Service> binding(&impl, std::move(request));
    ptr->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, nullptr,
                   called_cb);
    run_loop2.Run();
    EXPECT_TRUE(called);
    EXPECT_FALSE(encountered_error);
  }
  // Now that the Binding is out of scope we should detect an error on the other
  // end of the pipe.
  run_loop.Run();
  EXPECT_TRUE(encountered_error);

  // And calls should fail.
  called = false;
  ptr->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, nullptr,
                 called_cb);
  loop().RunUntilIdle();
  EXPECT_FALSE(called);
}

// Tests that the binding's connection error handler gets called when the other
// end is closed.
TEST_F(BindingTest, ConnectionError) {
  bool called = false;
  {
    ServiceImpl impl;
    sample::ServicePtr ptr;
    Binding<sample::Service> binding(&impl, GetProxy(&ptr));
    base::RunLoop run_loop;
    binding.set_connection_error_handler([&called, &run_loop]() {
      called = true;
      run_loop.Quit();
    });
    ptr.reset();
    EXPECT_FALSE(called);
    run_loop.Run();
    EXPECT_TRUE(called);
    // We want to make sure that it isn't called again during destruction.
    called = false;
  }
  EXPECT_FALSE(called);
}

// Tests that calling Close doesn't result in the connection error handler being
// called.
TEST_F(BindingTest, CloseDoesntCallConnectionErrorHandler) {
  ServiceImpl impl;
  sample::ServicePtr ptr;
  Binding<sample::Service> binding(&impl, GetProxy(&ptr));
  bool called = false;
  binding.set_connection_error_handler([&called]() { called = true; });
  binding.Close();
  loop().RunUntilIdle();
  EXPECT_FALSE(called);

  // We can also close the other end, and the error handler still won't be
  // called.
  ptr.reset();
  loop().RunUntilIdle();
  EXPECT_FALSE(called);
}

class ServiceImplWithBinding : public ServiceImpl {
 public:
  ServiceImplWithBinding(bool* was_deleted,
                         const base::Closure& closure,
                         InterfaceRequest<sample::Service> request)
      : ServiceImpl(was_deleted),
        binding_(this, std::move(request)),
        closure_(closure) {
    binding_.set_connection_error_handler([this]() { delete this; });
  }

 private:
  ~ServiceImplWithBinding() override{
    closure_.Run();
  }

  Binding<sample::Service> binding_;
  base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImplWithBinding);
};

// Tests that the binding may be deleted in the connection error handler.
TEST_F(BindingTest, SelfDeleteOnConnectionError) {
  bool was_deleted = false;
  sample::ServicePtr ptr;
  // This should delete itself on connection error.
  base::RunLoop run_loop;
  new ServiceImplWithBinding(&was_deleted, run_loop.QuitClosure(),
                             GetProxy(&ptr));
  ptr.reset();
  EXPECT_FALSE(was_deleted);
  run_loop.Run();
  EXPECT_TRUE(was_deleted);
}

// Tests that explicitly calling Unbind followed by rebinding works.
TEST_F(BindingTest, Unbind) {
  ServiceImpl impl;
  sample::ServicePtr ptr;
  Binding<sample::Service> binding(&impl, GetProxy(&ptr));

  bool called = false;
  base::RunLoop run_loop;
  auto called_cb = [&called, &run_loop](int32_t result) {
    called = true;
    run_loop.Quit();
  };
  ptr->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, nullptr,
                 called_cb);
  run_loop.Run();
  EXPECT_TRUE(called);

  called = false;
  auto request = binding.Unbind();
  EXPECT_FALSE(binding.is_bound());
  // All calls should fail when not bound...
  ptr->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, nullptr,
                 called_cb);
  loop().RunUntilIdle();
  EXPECT_FALSE(called);

  called = false;
  binding.Bind(std::move(request));
  EXPECT_TRUE(binding.is_bound());
  // ...and should succeed again when the rebound.
  base::RunLoop run_loop2;
  auto called_cb2 = [&called, &run_loop2](int32_t result) {
    called = true;
    run_loop2.Quit();
  };
  ptr->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, nullptr,
                 called_cb2);
  run_loop2.Run();
  EXPECT_TRUE(called);
}

class IntegerAccessorImpl : public sample::IntegerAccessor {
 public:
  IntegerAccessorImpl() {}
  ~IntegerAccessorImpl() override {}

 private:
  // sample::IntegerAccessor implementation.
  void GetInteger(const GetIntegerCallback& callback) override {
    callback.Run(1, sample::Enum::VALUE);
  }
  void SetInteger(int64_t data, sample::Enum type) override {}

  DISALLOW_COPY_AND_ASSIGN(IntegerAccessorImpl);
};

TEST_F(BindingTest, SetInterfacePtrVersion) {
  IntegerAccessorImpl impl;
  sample::IntegerAccessorPtr ptr;
  Binding<sample::IntegerAccessor> binding(&impl, &ptr);
  EXPECT_EQ(3u, ptr.version());
}

TEST_F(BindingTest, PauseResume) {
  bool called = false;
  base::RunLoop run_loop;
  auto called_cb = [&called, &run_loop](int32_t result) {
    called = true;
    run_loop.Quit();
  };
  sample::ServicePtr ptr;
  auto request = GetProxy(&ptr);
  ServiceImpl impl;
  Binding<sample::Service> binding(&impl, std::move(request));
  binding.PauseIncomingMethodCallProcessing();
  ptr->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, nullptr,
                 called_cb);
  EXPECT_FALSE(called);
  loop().RunUntilIdle();
  // Frobinate() should not be called as the binding is paused.
  EXPECT_FALSE(called);

  // Resume the binding, which should trigger processing.
  binding.ResumeIncomingMethodCallProcessing();
  run_loop.Run();
  EXPECT_TRUE(called);
}

// Verifies the connection error handler is not run while a binding is paused.
TEST_F(BindingTest, ErrorHandleNotRunWhilePaused) {
  bool called = false;
  base::RunLoop run_loop;
  sample::ServicePtr ptr;
  auto request = GetProxy(&ptr);
  ServiceImpl impl;
  Binding<sample::Service> binding(&impl, std::move(request));
  binding.set_connection_error_handler([&called, &run_loop]() {
      called = true;
      run_loop.Quit();
  });
  binding.PauseIncomingMethodCallProcessing();

  ptr.reset();
  loop().RunUntilIdle();
  // The connection error handle should not be called as the binding is paused.
  EXPECT_FALSE(called);

  // Resume the binding, which should trigger the error handler.
  binding.ResumeIncomingMethodCallProcessing();
  run_loop.Run();
  EXPECT_TRUE(called);
}

// StrongBindingTest -----------------------------------------------------------

using StrongBindingTest = BindingTestBase;

// Tests that destroying a mojo::StrongBinding closes the bound message pipe
// handle but does *not* destroy the implementation object.
TEST_F(StrongBindingTest, DestroyClosesMessagePipe) {
  base::RunLoop run_loop;
  bool encountered_error = false;
  bool was_deleted = false;
  ServiceImpl impl(&was_deleted);
  sample::ServicePtr ptr;
  auto request = GetProxy(&ptr);
  ptr.set_connection_error_handler(
      [&encountered_error, &run_loop]() {
        encountered_error = true;
        run_loop.Quit();
      });
  bool called = false;
  base::RunLoop run_loop2;
  auto called_cb = [&called, &run_loop2](int32_t result) {
    called = true;
    run_loop2.Quit();
  };
  {
    StrongBinding<sample::Service> binding(&impl, std::move(request));
    ptr->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, nullptr,
                   called_cb);
    run_loop2.Run();
    EXPECT_TRUE(called);
    EXPECT_FALSE(encountered_error);
  }
  // Now that the StrongBinding is out of scope we should detect an error on the
  // other end of the pipe.
  run_loop.Run();
  EXPECT_TRUE(encountered_error);
  // But destroying the StrongBinding doesn't destroy the object.
  ASSERT_FALSE(was_deleted);
}

class ServiceImplWithStrongBinding : public ServiceImpl {
 public:
  ServiceImplWithStrongBinding(bool* was_deleted,
                               InterfaceRequest<sample::Service> request)
      : ServiceImpl(was_deleted), binding_(this, std::move(request)) {}

  StrongBinding<sample::Service>& binding() { return binding_; }

 private:
  StrongBinding<sample::Service> binding_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImplWithStrongBinding);
};

// Tests the typical case, where the implementation object owns the
// StrongBinding (and should be destroyed on connection error).
TEST_F(StrongBindingTest, ConnectionErrorDestroysImpl) {
  sample::ServicePtr ptr;
  bool was_deleted = false;
  // Will delete itself.
  base::RunLoop run_loop;
  new ServiceImplWithBinding(&was_deleted, run_loop.QuitClosure(),
                             GetProxy(&ptr));

  loop().RunUntilIdle();
  EXPECT_FALSE(was_deleted);

  ptr.reset();
  EXPECT_FALSE(was_deleted);
  run_loop.Run();
  EXPECT_TRUE(was_deleted);
}

// Tests that even when the implementation object owns the StrongBinding, that
// the implementation can still be deleted (which should result in the message
// pipe being closed). Also checks that the connection error handler doesn't get
// called.
TEST_F(StrongBindingTest, ExplicitDeleteImpl) {
  bool ptr_error_handler_called = false;
  sample::ServicePtr ptr;
  auto request = GetProxy(&ptr);
  base::RunLoop run_loop;
  ptr.set_connection_error_handler(
      [&ptr_error_handler_called, &run_loop]() {
        ptr_error_handler_called = true;
        run_loop.Quit();
      });
  bool was_deleted = false;
  ServiceImplWithStrongBinding* impl =
      new ServiceImplWithStrongBinding(&was_deleted, std::move(request));
  bool binding_error_handler_called = false;
  impl->binding().set_connection_error_handler(
      [&binding_error_handler_called]() {
        binding_error_handler_called = true;
      });

  loop().RunUntilIdle();
  EXPECT_FALSE(ptr_error_handler_called);
  EXPECT_FALSE(was_deleted);

  delete impl;
  EXPECT_FALSE(ptr_error_handler_called);
  EXPECT_TRUE(was_deleted);
  was_deleted = false;  // It shouldn't be double-deleted!
  run_loop.Run();
  EXPECT_TRUE(ptr_error_handler_called);
  EXPECT_FALSE(was_deleted);

  EXPECT_FALSE(binding_error_handler_called);
}

}  // namespace
}  // mojo
