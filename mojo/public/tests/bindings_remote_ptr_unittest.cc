// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojo/public/tests/mojom/math_calculator.h"
#include "mojo/public/tests/simple_bindings_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

class MathCalculatorImpl : public math::CalculatorStub {
 public:
  explicit MathCalculatorImpl(Handle pipe)
      : ui_(pipe),
        total_(0.0) {
    ui_.SetPeer(this);
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

class MathCalculatorUIImpl : public math::CalculatorUIStub {
 public:
  explicit MathCalculatorUIImpl(Handle pipe)
      : calculator_(pipe),
        output_(0.0) {
    calculator_.SetPeer(this);
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

class BindingsRemotePtrTest : public testing::Test {
 public:
  BindingsRemotePtrTest() {
    CreateMessagePipe(&pipe0_, &pipe1_);
  }

  virtual ~BindingsRemotePtrTest() {
    Close(pipe0_);
    Close(pipe1_);
  }

  void PumpMessages() {
    bindings_support_.Process();
  }

 protected:
  Handle pipe0_;
  Handle pipe1_;

 private:
  SimpleBindingsSupport bindings_support_;
};

TEST_F(BindingsRemotePtrTest, EndToEnd) {
  // Suppose this is instantiated in a process that has pipe0_.
  MathCalculatorImpl calculator(pipe0_);

  // Suppose this is instantiated in a process that has pipe1_.
  MathCalculatorUIImpl calculator_ui(pipe1_);

  calculator_ui.Add(2.0);
  calculator_ui.Multiply(5.0);

  PumpMessages();

  EXPECT_EQ(10.0, calculator_ui.GetOutput());
}

}  // namespace test
}  // namespace mojo
