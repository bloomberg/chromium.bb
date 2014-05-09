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

  MathCalculatorImpl() : total_(0.0) {
  }

  virtual void OnConnectionError() MOJO_OVERRIDE {
    delete this;
  }

  virtual void SetClient(math::CalculatorUI* ui) MOJO_OVERRIDE {
    ui_ = ui;
  }

  virtual void Clear() MOJO_OVERRIDE {
    ui_->Output(total_);
  }

  virtual void Add(double value) MOJO_OVERRIDE {
    total_ += value;
    ui_->Output(total_);
  }

  virtual void Multiply(double value) MOJO_OVERRIDE {
    total_ *= value;
    ui_->Output(total_);
  }

 private:
  math::CalculatorUI* ui_;
  double total_;
};

class MathCalculatorUIImpl : public math::CalculatorUI {
 public:
  explicit MathCalculatorUIImpl(math::CalculatorPtr calculator)
      : calculator_(calculator.Pass()),
        output_(0.0) {
    calculator_->SetClient(this);
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
  BindToProxy(new MathCalculatorImpl(), &calc);

  // Suppose this is instantiated in a process that has pipe1_.
  MathCalculatorUIImpl calculator_ui(calc.Pass());

  calculator_ui.Add(2.0);
  calculator_ui.Multiply(5.0);

  PumpMessages();

  EXPECT_EQ(10.0, calculator_ui.GetOutput());
}

TEST_F(InterfacePtrTest, Movable) {
  math::CalculatorPtr a;
  math::CalculatorPtr b;
  BindToProxy(new MathCalculatorImpl(), &b);

  EXPECT_TRUE(!a.get());
  EXPECT_FALSE(!b.get());

  a = b.Pass();

  EXPECT_FALSE(!a.get());
  EXPECT_TRUE(!b.get());
}

TEST_F(InterfacePtrTest, Resettable) {
  math::CalculatorPtr a;

  EXPECT_TRUE(!a.get());

  MessagePipe pipe;

  // Save this so we can test it later.
  Handle handle = pipe.handle0.get();

  a = MakeProxy<math::Calculator>(pipe.handle0.Pass());

  EXPECT_FALSE(!a.get());

  a.reset();

  EXPECT_TRUE(!a.get());
  EXPECT_FALSE(a.internal_state()->router());

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

}  // namespace
}  // namespace test
}  // namespace mojo
