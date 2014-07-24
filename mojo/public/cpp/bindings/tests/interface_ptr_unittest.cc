// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/bindings/tests/math_calculator.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class ErrorObserver : public ErrorHandler {
 public:
  ErrorObserver() : encountered_error_(false) {
  }

  bool encountered_error() const { return encountered_error_; }

  virtual void OnConnectionError() MOJO_OVERRIDE {
    encountered_error_ = true;
  }

 private:
  bool encountered_error_;
};

class MathCalculatorImpl : public InterfaceImpl<math::Calculator> {
 public:
  virtual ~MathCalculatorImpl() {}

  MathCalculatorImpl()
      : total_(0.0),
        got_connection_(false) {
  }

  virtual void OnConnectionEstablished() MOJO_OVERRIDE {
    got_connection_ = true;
  }

  virtual void Clear() MOJO_OVERRIDE {
    client()->Output(total_);
  }

  virtual void Add(double value) MOJO_OVERRIDE {
    total_ += value;
    client()->Output(total_);
  }

  virtual void Multiply(double value) MOJO_OVERRIDE {
    total_ *= value;
    client()->Output(total_);
  }

  bool got_connection() const {
    return got_connection_;
  }

 private:
  double total_;
  bool got_connection_;
};

class MathCalculatorUIImpl : public math::CalculatorUI {
 public:
  explicit MathCalculatorUIImpl(math::CalculatorPtr calculator)
      : calculator_(calculator.Pass()),
        output_(0.0) {
    calculator_.set_client(this);
  }

  bool WaitForIncomingMethodCall() {
    return calculator_.WaitForIncomingMethodCall();
  }

  bool encountered_error() const {
    return calculator_.encountered_error();
  }

  void Add(double value) {
    calculator_->Add(value);
  }

  void Subtract(double value) {
    calculator_->Add(-value);
  }

  void Multiply(double value) {
    calculator_->Multiply(value);
  }

  void Divide(double value) {
    calculator_->Multiply(1.0 / value);
  }

  double GetOutput() const {
    return output_;
  }

 private:
  // math::CalculatorUI implementation:
  virtual void Output(double value) MOJO_OVERRIDE {
    output_ = value;
  }

  math::CalculatorPtr calculator_;
  double output_;
};

class SelfDestructingMathCalculatorUIImpl : public math::CalculatorUI {
 public:
  explicit SelfDestructingMathCalculatorUIImpl(math::CalculatorPtr calculator)
      : calculator_(calculator.Pass()),
        nesting_level_(0) {
    ++num_instances_;
    calculator_.set_client(this);
  }

  void BeginTest(bool nested) {
    nesting_level_ = nested ? 2 : 1;
    calculator_->Add(1.0);
  }

  static int num_instances() { return num_instances_; }

 private:
  virtual ~SelfDestructingMathCalculatorUIImpl() {
    --num_instances_;
  }

  virtual void Output(double value) MOJO_OVERRIDE {
    if (--nesting_level_ > 0) {
      // Add some more and wait for re-entrant call to Output!
      calculator_->Add(1.0);
      RunLoop::current()->RunUntilIdle();
    } else {
      delete this;
    }
  }

  math::CalculatorPtr calculator_;
  int nesting_level_;
  static int num_instances_;
};

// static
int SelfDestructingMathCalculatorUIImpl::num_instances_ = 0;

class ReentrantServiceImpl : public InterfaceImpl<sample::Service> {
 public:
  virtual ~ReentrantServiceImpl() {}

  ReentrantServiceImpl()
      : got_connection_(false), call_depth_(0), max_call_depth_(0) {}

  virtual void OnConnectionEstablished() MOJO_OVERRIDE {
    got_connection_ = true;
  }

  bool got_connection() const {
    return got_connection_;
  }

  int max_call_depth() { return max_call_depth_; }

  virtual void Frobinate(sample::FooPtr foo,
                         sample::Service::BazOptions baz,
                         sample::PortPtr port) MOJO_OVERRIDE {
    max_call_depth_ = std::max(++call_depth_, max_call_depth_);
    if (call_depth_ == 1) {
      EXPECT_TRUE(WaitForIncomingMethodCall());
    }
    call_depth_--;
  }

  virtual void GetPort(mojo::InterfaceRequest<sample::Port> port)
      MOJO_OVERRIDE {}

 private:
  bool got_connection_;
  int call_depth_;
  int max_call_depth_;
};

class InterfacePtrTest : public testing::Test {
 public:
  virtual ~InterfacePtrTest() {
    loop_.RunUntilIdle();
  }

  void PumpMessages() {
    loop_.RunUntilIdle();
  }

 private:
  Environment env_;
  RunLoop loop_;
};

TEST_F(InterfacePtrTest, EndToEnd) {
  math::CalculatorPtr calc;
  MathCalculatorImpl* impl = BindToProxy(new MathCalculatorImpl(), &calc);
  EXPECT_TRUE(impl->got_connection());

  // Suppose this is instantiated in a process that has pipe1_.
  MathCalculatorUIImpl calculator_ui(calc.Pass());

  calculator_ui.Add(2.0);
  calculator_ui.Multiply(5.0);

  PumpMessages();

  EXPECT_EQ(10.0, calculator_ui.GetOutput());
}

TEST_F(InterfacePtrTest, EndToEnd_Synchronous) {
  math::CalculatorPtr calc;
  MathCalculatorImpl* impl = BindToProxy(new MathCalculatorImpl(), &calc);
  EXPECT_TRUE(impl->got_connection());

  // Suppose this is instantiated in a process that has pipe1_.
  MathCalculatorUIImpl calculator_ui(calc.Pass());

  EXPECT_EQ(0.0, calculator_ui.GetOutput());

  calculator_ui.Add(2.0);
  EXPECT_EQ(0.0, calculator_ui.GetOutput());
  impl->WaitForIncomingMethodCall();
  calculator_ui.WaitForIncomingMethodCall();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());

  calculator_ui.Multiply(5.0);
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  impl->WaitForIncomingMethodCall();
  calculator_ui.WaitForIncomingMethodCall();
  EXPECT_EQ(10.0, calculator_ui.GetOutput());
}

TEST_F(InterfacePtrTest, Movable) {
  math::CalculatorPtr a;
  math::CalculatorPtr b;
  BindToProxy(new MathCalculatorImpl(), &b);

  EXPECT_TRUE(!a);
  EXPECT_FALSE(!b);

  a = b.Pass();

  EXPECT_FALSE(!a);
  EXPECT_TRUE(!b);
}

TEST_F(InterfacePtrTest, Resettable) {
  math::CalculatorPtr a;

  EXPECT_TRUE(!a);

  MessagePipe pipe;

  // Save this so we can test it later.
  Handle handle = pipe.handle0.get();

  a = MakeProxy<math::Calculator>(pipe.handle0.Pass());

  EXPECT_FALSE(!a);

  a.reset();

  EXPECT_TRUE(!a);
  EXPECT_FALSE(a.internal_state()->router_for_testing());

  // Test that handle was closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, CloseRaw(handle));
}

TEST_F(InterfacePtrTest, EncounteredError) {
  math::CalculatorPtr proxy;
  MathCalculatorImpl* server = BindToProxy(new MathCalculatorImpl(), &proxy);

  MathCalculatorUIImpl calculator_ui(proxy.Pass());

  calculator_ui.Add(2.0);
  PumpMessages();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  EXPECT_FALSE(calculator_ui.encountered_error());

  calculator_ui.Multiply(5.0);
  EXPECT_FALSE(calculator_ui.encountered_error());

  // Close the server.
  server->internal_state()->router()->CloseMessagePipe();

  // The state change isn't picked up locally yet.
  EXPECT_FALSE(calculator_ui.encountered_error());

  PumpMessages();

  // OK, now we see the error.
  EXPECT_TRUE(calculator_ui.encountered_error());
}

TEST_F(InterfacePtrTest, EncounteredErrorCallback) {
  math::CalculatorPtr proxy;
  MathCalculatorImpl* server = BindToProxy(new MathCalculatorImpl(), &proxy);

  ErrorObserver error_observer;
  proxy.set_error_handler(&error_observer);

  MathCalculatorUIImpl calculator_ui(proxy.Pass());

  calculator_ui.Add(2.0);
  PumpMessages();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  EXPECT_FALSE(calculator_ui.encountered_error());

  calculator_ui.Multiply(5.0);
  EXPECT_FALSE(calculator_ui.encountered_error());

  // Close the server.
  server->internal_state()->router()->CloseMessagePipe();

  // The state change isn't picked up locally yet.
  EXPECT_FALSE(calculator_ui.encountered_error());

  PumpMessages();

  // OK, now we see the error.
  EXPECT_TRUE(calculator_ui.encountered_error());

  // We should have also been able to observe the error through the
  // ErrorHandler interface.
  EXPECT_TRUE(error_observer.encountered_error());
}

TEST_F(InterfacePtrTest, NoClientAttribute) {
  // This is a test to ensure the following compiles. The sample::Port interface
  // does not have an explicit Client attribute.
  sample::PortPtr port;
  MessagePipe pipe;
  port.Bind(pipe.handle0.Pass());
}

TEST_F(InterfacePtrTest, DestroyInterfacePtrOnClientMethod) {
  math::CalculatorPtr proxy;
  BindToProxy(new MathCalculatorImpl(), &proxy);

  EXPECT_EQ(0, SelfDestructingMathCalculatorUIImpl::num_instances());

  SelfDestructingMathCalculatorUIImpl* impl =
      new SelfDestructingMathCalculatorUIImpl(proxy.Pass());
  impl->BeginTest(false);

  PumpMessages();

  EXPECT_EQ(0, SelfDestructingMathCalculatorUIImpl::num_instances());
}

TEST_F(InterfacePtrTest, NestedDestroyInterfacePtrOnClientMethod) {
  math::CalculatorPtr proxy;
  BindToProxy(new MathCalculatorImpl(), &proxy);

  EXPECT_EQ(0, SelfDestructingMathCalculatorUIImpl::num_instances());

  SelfDestructingMathCalculatorUIImpl* impl =
      new SelfDestructingMathCalculatorUIImpl(proxy.Pass());
  impl->BeginTest(true);

  PumpMessages();

  EXPECT_EQ(0, SelfDestructingMathCalculatorUIImpl::num_instances());
}

TEST_F(InterfacePtrTest, ReentrantWaitForIncomingMethodCall) {
  sample::ServicePtr proxy;
  ReentrantServiceImpl* impl = BindToProxy(new ReentrantServiceImpl(), &proxy);
  EXPECT_TRUE(impl->got_connection());

  proxy->Frobinate(sample::FooPtr(),
                   sample::Service::BAZ_OPTIONS_REGULAR,
                   sample::PortPtr());
  proxy->Frobinate(sample::FooPtr(),
                   sample::Service::BAZ_OPTIONS_REGULAR,
                   sample::PortPtr());

  PumpMessages();

  EXPECT_EQ(2, impl->max_call_depth());
}

}  // namespace
}  // namespace test
}  // namespace mojo
