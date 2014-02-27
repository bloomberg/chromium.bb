// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/error_handler.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/bindings/tests/math_calculator.mojom.h"
#include "mojo/public/bindings/tests/sample_service.mojom.h"
#include "mojo/public/environment/environment.h"
#include "mojo/public/utility/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class ErrorObserver : public ErrorHandler {
 public:
  ErrorObserver() : encountered_error_(false) {
  }

  bool encountered_error() const { return encountered_error_; }

  virtual void OnError() MOJO_OVERRIDE {
    encountered_error_ = true;
  }

 private:
  bool encountered_error_;
};

class MathCalculatorImpl : public math::Calculator {
 public:
  virtual ~MathCalculatorImpl() {}

  explicit MathCalculatorImpl(math::ScopedCalculatorUIHandle ui_handle)
      : ui_(ui_handle.Pass(), this),
        total_(0.0) {
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
  RemotePtr<math::CalculatorUI> ui_;
  double total_;
};

class MathCalculatorUIImpl : public math::CalculatorUI {
 public:
  explicit MathCalculatorUIImpl(math::ScopedCalculatorHandle calculator_handle,
                                ErrorHandler* error_handler = NULL)
      : calculator_(calculator_handle.Pass(), this, error_handler),
        output_(0.0) {
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

  RemotePtr<math::Calculator> calculator_;
  double output_;
};

class RemotePtrTest : public testing::Test {
 public:
  void PumpMessages() {
    loop_.RunUntilIdle();
  }

 protected:
  InterfacePipe<math::CalculatorUI> pipe_;

 private:
  Environment env_;
  RunLoop loop_;
};

}  // namespace

TEST_F(RemotePtrTest, EndToEnd) {
  // Suppose this is instantiated in a process that has pipe0_.
  MathCalculatorImpl calculator(pipe_.handle_to_self.Pass());

  // Suppose this is instantiated in a process that has pipe1_.
  MathCalculatorUIImpl calculator_ui(pipe_.handle_to_peer.Pass());

  calculator_ui.Add(2.0);
  calculator_ui.Multiply(5.0);

  PumpMessages();

  EXPECT_EQ(10.0, calculator_ui.GetOutput());
}

TEST_F(RemotePtrTest, Movable) {
  RemotePtr<math::Calculator> a;
  RemotePtr<math::Calculator> b(pipe_.handle_to_peer.Pass(), NULL);

  EXPECT_TRUE(a.is_null());
  EXPECT_FALSE(b.is_null());

  a = b.Pass();

  EXPECT_FALSE(a.is_null());
  EXPECT_TRUE(b.is_null());
}

TEST_F(RemotePtrTest, Resettable) {
  RemotePtr<math::Calculator> a;

  EXPECT_TRUE(a.is_null());

  math::CalculatorHandle handle = pipe_.handle_to_peer.get();

  a.reset(pipe_.handle_to_peer.Pass(), NULL);

  EXPECT_FALSE(a.is_null());

  a.reset();

  EXPECT_TRUE(a.is_null());

  // Test that handle was closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, CloseRaw(handle));
}

TEST_F(RemotePtrTest, EncounteredError) {
  MathCalculatorImpl* calculator =
      new MathCalculatorImpl(pipe_.handle_to_self.Pass());

  MathCalculatorUIImpl calculator_ui(pipe_.handle_to_peer.Pass());

  calculator_ui.Add(2.0);
  PumpMessages();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  EXPECT_FALSE(calculator_ui.encountered_error());

  calculator_ui.Multiply(5.0);
  EXPECT_FALSE(calculator_ui.encountered_error());

  // Close the other side of the pipe.
  delete calculator;

  // The state change isn't picked up locally yet.
  EXPECT_FALSE(calculator_ui.encountered_error());

  PumpMessages();

  // OK, now we see the error.
  EXPECT_TRUE(calculator_ui.encountered_error());
}

TEST_F(RemotePtrTest, EncounteredErrorCallback) {
  MathCalculatorImpl* calculator =
      new MathCalculatorImpl(pipe_.handle_to_self.Pass());

  ErrorObserver error_observer;
  MathCalculatorUIImpl calculator_ui(pipe_.handle_to_peer.Pass(),
                                     &error_observer);

  calculator_ui.Add(2.0);
  PumpMessages();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  EXPECT_FALSE(calculator_ui.encountered_error());

  calculator_ui.Multiply(5.0);
  EXPECT_FALSE(calculator_ui.encountered_error());

  // Close the other side of the pipe.
  delete calculator;

  // The state change isn't picked up locally yet.
  EXPECT_FALSE(calculator_ui.encountered_error());

  PumpMessages();

  // OK, now we see the error.
  EXPECT_TRUE(calculator_ui.encountered_error());

  // We should have also been able to observe the error through the
  // ErrorHandler interface.
  EXPECT_TRUE(error_observer.encountered_error());
}

TEST_F(RemotePtrTest, NoPeerAttribute) {
  // This is a test to ensure the following compiles. The sample::Port interface
  // does not have an explicit Peer attribute.
  InterfacePipe<sample::Port, NoInterface> pipe;
  RemotePtr<sample::Port> port(pipe.handle_to_self.Pass());
}

}  // namespace test
}  // namespace mojo
