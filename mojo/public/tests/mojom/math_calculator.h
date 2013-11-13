// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_MATH_CALCULATOR_MATH_CALCULATOR_H_
#define MOJO_GENERATED_BINDINGS_MATH_CALCULATOR_MATH_CALCULATOR_H_

#include "mojo/public/bindings/lib/bindings.h"
#include "mojo/public/bindings/lib/message.h"

namespace math {

#pragma pack(push, 1)


#pragma pack(pop)

class CalculatorProxy;
class CalculatorStub;
class CalculatorUI;

class Calculator {
 public:
  typedef CalculatorProxy _Proxy;
  typedef CalculatorStub _Stub;
  typedef CalculatorUI _Peer;
  virtual void Clear()  = 0;
  virtual void Add(double value)  = 0;
  virtual void Multiply(double value)  = 0;
};

class CalculatorUIProxy;
class CalculatorUIStub;
class Calculator;

class CalculatorUI {
 public:
  typedef CalculatorUIProxy _Proxy;
  typedef CalculatorUIStub _Stub;
  typedef Calculator _Peer;
  virtual void Output(double value)  = 0;
};

class CalculatorProxy : public Calculator {
 public:
  explicit CalculatorProxy(mojo::MessageReceiver* receiver);

  virtual void Clear() MOJO_OVERRIDE;
  virtual void Add(double value) MOJO_OVERRIDE;
  virtual void Multiply(double value) MOJO_OVERRIDE;

 private:
  mojo::MessageReceiver* receiver_;
};

class CalculatorUIProxy : public CalculatorUI {
 public:
  explicit CalculatorUIProxy(mojo::MessageReceiver* receiver);

  virtual void Output(double value) MOJO_OVERRIDE;

 private:
  mojo::MessageReceiver* receiver_;
};

class CalculatorStub : public Calculator, public mojo::MessageReceiver {
 public:
  virtual bool Accept(mojo::Message* message) MOJO_OVERRIDE;
};

class CalculatorUIStub : public CalculatorUI, public mojo::MessageReceiver {
 public:
  virtual bool Accept(mojo::Message* message) MOJO_OVERRIDE;
};


}  // namespace math

#endif  // MOJO_GENERATED_BINDINGS_MATH_CALCULATOR_MATH_CALCULATOR_H_
