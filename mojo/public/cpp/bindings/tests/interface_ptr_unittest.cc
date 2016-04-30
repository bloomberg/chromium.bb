// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/interfaces/bindings/tests/math_calculator.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom.h"
#include "mojo/public/interfaces/bindings/tests/scoping.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

template <typename Method, typename Class>
class RunnableImpl {
 public:
  RunnableImpl(Method method, Class instance)
      : method_(method), instance_(instance) {}
  template <typename... Args>
  void Run(Args... args) const {
    (instance_->*method_)(args...);
  }

 private:
  Method method_;
  Class instance_;
};

template <typename Method, typename Class>
RunnableImpl<Method, Class> MakeRunnable(Method method, Class object) {
  return RunnableImpl<Method, Class>(method, object);
}

typedef mojo::Callback<void(double)> CalcCallback;

class MathCalculatorImpl : public math::Calculator {
 public:
  explicit MathCalculatorImpl(InterfaceRequest<math::Calculator> request)
      : total_(0.0), binding_(this, std::move(request)) {}
  ~MathCalculatorImpl() override {}

  void CloseMessagePipe() { binding_.Close(); }

  void WaitForIncomingMethodCall() { binding_.WaitForIncomingMethodCall(); }

  void Clear(const CalcCallback& callback) override {
    total_ = 0.0;
    callback.Run(total_);
  }

  void Add(double value, const CalcCallback& callback) override {
    total_ += value;
    callback.Run(total_);
  }

  void Multiply(double value, const CalcCallback& callback) override {
    total_ *= value;
    callback.Run(total_);
  }

 private:
  double total_;
  Binding<math::Calculator> binding_;
};

class MathCalculatorUI {
 public:
  explicit MathCalculatorUI(math::CalculatorPtr calculator)
      : calculator_(std::move(calculator)),
        output_(0.0) {}

  bool WaitForIncomingResponse() {
    return calculator_.WaitForIncomingResponse();
  }

  bool encountered_error() const { return calculator_.encountered_error(); }
  void set_connection_error_handler(const base::Closure& closure) {
    calculator_.set_connection_error_handler(closure);
  }

  void Add(double value, const base::Closure& closure) {
    calculator_->Add(
        value,
        base::Bind(&MathCalculatorUI::Output, base::Unretained(this), closure));
  }

  void Multiply(double value, const base::Closure& closure) {
    calculator_->Multiply(
        value,
        base::Bind(&MathCalculatorUI::Output, base::Unretained(this), closure));
  }

  double GetOutput() const { return output_; }

 private:
  void Output(const base::Closure& closure, double output) {
    output_ = output;
    if (!closure.is_null())
      closure.Run();
  }

  math::CalculatorPtr calculator_;
  double output_;
  base::Closure closure_;
};

class SelfDestructingMathCalculatorUI {
 public:
  explicit SelfDestructingMathCalculatorUI(math::CalculatorPtr calculator)
      : calculator_(std::move(calculator)), nesting_level_(0) {
    ++num_instances_;
  }

  void BeginTest(bool nested, const base::Closure& closure) {
    nesting_level_ = nested ? 2 : 1;
    calculator_->Add(
        1.0,
        base::Bind(&SelfDestructingMathCalculatorUI::Output,
                   base::Unretained(this), closure));
  }

  static int num_instances() { return num_instances_; }

  void Output(const base::Closure& closure, double value) {
    if (--nesting_level_ > 0) {
      // Add some more and wait for re-entrant call to Output!
      calculator_->Add(
          1.0,
          base::Bind(&SelfDestructingMathCalculatorUI::Output,
                     base::Unretained(this), closure));
    } else {
      closure.Run();
      delete this;
    }
  }

 private:
  ~SelfDestructingMathCalculatorUI() { --num_instances_; }

  math::CalculatorPtr calculator_;
  int nesting_level_;
  static int num_instances_;
};

// static
int SelfDestructingMathCalculatorUI::num_instances_ = 0;

class ReentrantServiceImpl : public sample::Service {
 public:
  ~ReentrantServiceImpl() override {}

  explicit ReentrantServiceImpl(InterfaceRequest<sample::Service> request)
      : call_depth_(0),
        max_call_depth_(0),
        binding_(this, std::move(request)) {}

  int max_call_depth() { return max_call_depth_; }

  void Frobinate(sample::FooPtr foo,
                 sample::Service::BazOptions baz,
                 sample::PortPtr port,
                 const sample::Service::FrobinateCallback& callback) override {
    max_call_depth_ = std::max(++call_depth_, max_call_depth_);
    if (call_depth_ == 1) {
      EXPECT_TRUE(binding_.WaitForIncomingMethodCall());
    }
    call_depth_--;
    callback.Run(5);
  }

  void GetPort(mojo::InterfaceRequest<sample::Port> port) override {}

 private:
  int call_depth_;
  int max_call_depth_;
  Binding<sample::Service> binding_;
};

class IntegerAccessorImpl : public sample::IntegerAccessor {
 public:
  IntegerAccessorImpl() : integer_(0) {}
  ~IntegerAccessorImpl() override {}

  int64_t integer() const { return integer_; }

  void set_closure(const base::Closure& closure) { closure_ = closure; }

 private:
  // sample::IntegerAccessor implementation.
  void GetInteger(const GetIntegerCallback& callback) override {
    callback.Run(integer_, sample::Enum::VALUE);
  }
  void SetInteger(int64_t data, sample::Enum type) override {
    integer_ = data;
    if (!closure_.is_null()) {
      closure_.Run();
      closure_.Reset();
    }
  }

  int64_t integer_;
  base::Closure closure_;
};

class InterfacePtrTest : public testing::Test {
 public:
  InterfacePtrTest() : loop_(common::MessagePumpMojo::Create()) {}
  ~InterfacePtrTest() override { loop_.RunUntilIdle(); }

  void PumpMessages() { loop_.RunUntilIdle(); }

 private:
  base::MessageLoop loop_;
};

TEST_F(InterfacePtrTest, IsBound) {
  math::CalculatorPtr calc;
  EXPECT_FALSE(calc.is_bound());
  MathCalculatorImpl calc_impl(GetProxy(&calc));
  EXPECT_TRUE(calc.is_bound());
}

TEST_F(InterfacePtrTest, EndToEnd) {
  math::CalculatorPtr calc;
  MathCalculatorImpl calc_impl(GetProxy(&calc));

  // Suppose this is instantiated in a process that has pipe1_.
  MathCalculatorUI calculator_ui(std::move(calc));

  base::RunLoop run_loop, run_loop2;
  calculator_ui.Add(2.0, run_loop.QuitClosure());
  calculator_ui.Multiply(5.0, run_loop2.QuitClosure());
  run_loop.Run();
  run_loop2.Run();

  EXPECT_EQ(10.0, calculator_ui.GetOutput());
}

TEST_F(InterfacePtrTest, EndToEnd_Synchronous) {
  math::CalculatorPtr calc;
  MathCalculatorImpl calc_impl(GetProxy(&calc));

  // Suppose this is instantiated in a process that has pipe1_.
  MathCalculatorUI calculator_ui(std::move(calc));

  EXPECT_EQ(0.0, calculator_ui.GetOutput());

  calculator_ui.Add(2.0, base::Closure());
  EXPECT_EQ(0.0, calculator_ui.GetOutput());
  calc_impl.WaitForIncomingMethodCall();
  calculator_ui.WaitForIncomingResponse();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());

  calculator_ui.Multiply(5.0, base::Closure());
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  calc_impl.WaitForIncomingMethodCall();
  calculator_ui.WaitForIncomingResponse();
  EXPECT_EQ(10.0, calculator_ui.GetOutput());
}

TEST_F(InterfacePtrTest, Movable) {
  math::CalculatorPtr a;
  math::CalculatorPtr b;
  MathCalculatorImpl calc_impl(GetProxy(&b));

  EXPECT_TRUE(!a);
  EXPECT_FALSE(!b);

  a = std::move(b);

  EXPECT_FALSE(!a);
  EXPECT_TRUE(!b);
}

TEST_F(InterfacePtrTest, Resettable) {
  math::CalculatorPtr a;

  EXPECT_TRUE(!a);

  MessagePipe pipe;

  // Save this so we can test it later.
  Handle handle = pipe.handle0.get();

  a = MakeProxy(
      InterfacePtrInfo<math::Calculator>(std::move(pipe.handle0), 0u));

  EXPECT_FALSE(!a);

  a.reset();

  EXPECT_TRUE(!a);
  EXPECT_FALSE(a.internal_state()->is_bound());

  // Test that handle was closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, CloseRaw(handle));
}

TEST_F(InterfacePtrTest, BindInvalidHandle) {
  math::CalculatorPtr ptr;
  EXPECT_FALSE(ptr.get());
  EXPECT_FALSE(ptr);

  ptr.Bind(InterfacePtrInfo<math::Calculator>());
  EXPECT_FALSE(ptr.get());
  EXPECT_FALSE(ptr);
}

TEST_F(InterfacePtrTest, EncounteredError) {
  math::CalculatorPtr proxy;
  MathCalculatorImpl calc_impl(GetProxy(&proxy));

  MathCalculatorUI calculator_ui(std::move(proxy));

  base::RunLoop run_loop;
  calculator_ui.Add(2.0, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  EXPECT_FALSE(calculator_ui.encountered_error());

  calculator_ui.Multiply(5.0, base::Closure());
  EXPECT_FALSE(calculator_ui.encountered_error());

  // Close the server.
  calc_impl.CloseMessagePipe();

  // The state change isn't picked up locally yet.
  base::RunLoop run_loop2;
  calculator_ui.set_connection_error_handler(run_loop2.QuitClosure());
  EXPECT_FALSE(calculator_ui.encountered_error());

  run_loop2.Run();

  // OK, now we see the error.
  EXPECT_TRUE(calculator_ui.encountered_error());
}

TEST_F(InterfacePtrTest, EncounteredErrorCallback) {
  math::CalculatorPtr proxy;
  MathCalculatorImpl calc_impl(GetProxy(&proxy));

  bool encountered_error = false;
  base::RunLoop run_loop;
  proxy.set_connection_error_handler(
      [&encountered_error, &run_loop]() {
        encountered_error = true;
        run_loop.Quit();
      });

  MathCalculatorUI calculator_ui(std::move(proxy));

  base::RunLoop run_loop2;
  calculator_ui.Add(2.0, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  EXPECT_FALSE(calculator_ui.encountered_error());

  calculator_ui.Multiply(5.0, base::Closure());
  EXPECT_FALSE(calculator_ui.encountered_error());

  // Close the server.
  calc_impl.CloseMessagePipe();

  // The state change isn't picked up locally yet.
  EXPECT_FALSE(calculator_ui.encountered_error());

  run_loop.Run();

  // OK, now we see the error.
  EXPECT_TRUE(calculator_ui.encountered_error());

  // We should have also been able to observe the error through the error
  // handler.
  EXPECT_TRUE(encountered_error);
}

TEST_F(InterfacePtrTest, DestroyInterfacePtrOnMethodResponse) {
  math::CalculatorPtr proxy;
  MathCalculatorImpl calc_impl(GetProxy(&proxy));

  EXPECT_EQ(0, SelfDestructingMathCalculatorUI::num_instances());

  SelfDestructingMathCalculatorUI* impl =
      new SelfDestructingMathCalculatorUI(std::move(proxy));
  base::RunLoop run_loop;
  impl->BeginTest(false, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(0, SelfDestructingMathCalculatorUI::num_instances());
}

TEST_F(InterfacePtrTest, NestedDestroyInterfacePtrOnMethodResponse) {
  math::CalculatorPtr proxy;
  MathCalculatorImpl calc_impl(GetProxy(&proxy));

  EXPECT_EQ(0, SelfDestructingMathCalculatorUI::num_instances());

  SelfDestructingMathCalculatorUI* impl =
      new SelfDestructingMathCalculatorUI(std::move(proxy));
  base::RunLoop run_loop;
  impl->BeginTest(true, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(0, SelfDestructingMathCalculatorUI::num_instances());
}

TEST_F(InterfacePtrTest, ReentrantWaitForIncomingMethodCall) {
  sample::ServicePtr proxy;
  ReentrantServiceImpl impl(GetProxy(&proxy));

  base::RunLoop run_loop, run_loop2;
  auto called_cb = [&run_loop](int32_t result) { run_loop.Quit(); };
  auto called_cb2 = [&run_loop2](int32_t result) { run_loop2.Quit(); };
  proxy->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, nullptr,
                   called_cb);
  proxy->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, nullptr,
                   called_cb2);

  run_loop.Run();
  run_loop2.Run();

  EXPECT_EQ(2, impl.max_call_depth());
}

TEST_F(InterfacePtrTest, QueryVersion) {
  IntegerAccessorImpl impl;
  sample::IntegerAccessorPtr ptr;
  Binding<sample::IntegerAccessor> binding(&impl, GetProxy(&ptr));

  EXPECT_EQ(0u, ptr.version());

  base::RunLoop run_loop;
  auto callback = [&run_loop](uint32_t version) {
    EXPECT_EQ(3u, version);
    run_loop.Quit();
  };
  ptr.QueryVersion(callback);

  run_loop.Run();

  EXPECT_EQ(3u, ptr.version());
}

TEST_F(InterfacePtrTest, RequireVersion) {
  IntegerAccessorImpl impl;
  sample::IntegerAccessorPtr ptr;
  Binding<sample::IntegerAccessor> binding(&impl, GetProxy(&ptr));

  EXPECT_EQ(0u, ptr.version());

  ptr.RequireVersion(1u);
  EXPECT_EQ(1u, ptr.version());
  base::RunLoop run_loop;
  impl.set_closure(run_loop.QuitClosure());
  ptr->SetInteger(123, sample::Enum::VALUE);
  run_loop.Run();
  EXPECT_FALSE(ptr.encountered_error());
  EXPECT_EQ(123, impl.integer());

  ptr.RequireVersion(3u);
  EXPECT_EQ(3u, ptr.version());
  base::RunLoop run_loop2;
  impl.set_closure(run_loop2.QuitClosure());
  ptr->SetInteger(456, sample::Enum::VALUE);
  run_loop2.Run();
  EXPECT_FALSE(ptr.encountered_error());
  EXPECT_EQ(456, impl.integer());

  // Require a version that is not supported by the impl side.
  ptr.RequireVersion(4u);
  // This value is set to the input of RequireVersion() synchronously.
  EXPECT_EQ(4u, ptr.version());
  base::RunLoop run_loop3;
  ptr.set_connection_error_handler(run_loop3.QuitClosure());
  ptr->SetInteger(789, sample::Enum::VALUE);
  run_loop3.Run();
  EXPECT_TRUE(ptr.encountered_error());
  // The call to SetInteger() after RequireVersion(4u) is ignored.
  EXPECT_EQ(456, impl.integer());
}

class StrongMathCalculatorImpl : public math::Calculator {
 public:
  StrongMathCalculatorImpl(ScopedMessagePipeHandle handle,
                           bool* error_received,
                           bool* destroyed,
                           const base::Closure& closure)
      : error_received_(error_received),
        destroyed_(destroyed),
        closure_(closure),
        binding_(this, std::move(handle)) {
    binding_.set_connection_error_handler(
        [this]() { *error_received_ = true; closure_.Run(); });
  }
  ~StrongMathCalculatorImpl() override { *destroyed_ = true; }

  // math::Calculator implementation.
  void Clear(const CalcCallback& callback) override { callback.Run(total_); }

  void Add(double value, const CalcCallback& callback) override {
    total_ += value;
    callback.Run(total_);
  }

  void Multiply(double value, const CalcCallback& callback) override {
    total_ *= value;
    callback.Run(total_);
  }

 private:
  double total_ = 0.0;
  bool* error_received_;
  bool* destroyed_;
  base::Closure closure_;

  StrongBinding<math::Calculator> binding_;
};

TEST(StrongConnectorTest, Math) {
  base::MessageLoop loop(common::MessagePumpMojo::Create());

  bool error_received = false;
  bool destroyed = false;
  MessagePipe pipe;
  base::RunLoop run_loop;
  new StrongMathCalculatorImpl(std::move(pipe.handle0), &error_received,
                               &destroyed, run_loop.QuitClosure());

  math::CalculatorPtr calc;
  calc.Bind(InterfacePtrInfo<math::Calculator>(std::move(pipe.handle1), 0u));

  {
    // Suppose this is instantiated in a process that has the other end of the
    // message pipe.
    MathCalculatorUI calculator_ui(std::move(calc));

    base::RunLoop run_loop, run_loop2;
    calculator_ui.Add(2.0, run_loop.QuitClosure());
    calculator_ui.Multiply(5.0, run_loop2.QuitClosure());
    run_loop.Run();
    run_loop2.Run();

    EXPECT_EQ(10.0, calculator_ui.GetOutput());
    EXPECT_FALSE(error_received);
    EXPECT_FALSE(destroyed);
  }
  // Destroying calculator_ui should close the pipe and generate an error on the
  // other
  // end which will destroy the instance since it is strongly bound.

  run_loop.Run();
  EXPECT_TRUE(error_received);
  EXPECT_TRUE(destroyed);
}

class WeakMathCalculatorImpl : public math::Calculator {
 public:
  WeakMathCalculatorImpl(ScopedMessagePipeHandle handle,
                         bool* error_received,
                         bool* destroyed,
                         const base::Closure& closure)
      : error_received_(error_received),
        destroyed_(destroyed),
        closure_(closure),
        binding_(this, std::move(handle)) {
    binding_.set_connection_error_handler(
        [this]() { *error_received_ = true; closure_.Run(); });
  }
  ~WeakMathCalculatorImpl() override { *destroyed_ = true; }

  void Clear(const CalcCallback& callback) override { callback.Run(total_); }

  void Add(double value, const CalcCallback& callback) override {
    total_ += value;
    callback.Run(total_);
  }

  void Multiply(double value, const CalcCallback& callback) override {
    total_ *= value;
    callback.Run(total_);
  }

 private:
  double total_ = 0.0;
  bool* error_received_;
  bool* destroyed_;
  base::Closure closure_;

  Binding<math::Calculator> binding_;
};

TEST(WeakConnectorTest, Math) {
  base::MessageLoop loop(common::MessagePumpMojo::Create());

  bool error_received = false;
  bool destroyed = false;
  MessagePipe pipe;
  base::RunLoop run_loop;
  WeakMathCalculatorImpl impl(std::move(pipe.handle0), &error_received,
                              &destroyed, run_loop.QuitClosure());

  math::CalculatorPtr calc;
  calc.Bind(InterfacePtrInfo<math::Calculator>(std::move(pipe.handle1), 0u));

  {
    // Suppose this is instantiated in a process that has the other end of the
    // message pipe.
    MathCalculatorUI calculator_ui(std::move(calc));

    base::RunLoop run_loop, run_loop2;
    calculator_ui.Add(2.0, run_loop.QuitClosure());
    calculator_ui.Multiply(5.0, run_loop2.QuitClosure());
    run_loop.Run();
    run_loop2.Run();

    EXPECT_EQ(10.0, calculator_ui.GetOutput());
    EXPECT_FALSE(error_received);
    EXPECT_FALSE(destroyed);
    // Destroying calculator_ui should close the pipe and generate an error on
    // the other
    // end which will destroy the instance since it is strongly bound.
  }

  run_loop.Run();
  EXPECT_TRUE(error_received);
  EXPECT_FALSE(destroyed);
}

class CImpl : public C {
 public:
  CImpl(bool* d_called, InterfaceRequest<C> request,
        const base::Closure& closure)
      : d_called_(d_called), binding_(this, std::move(request)),
        closure_(closure) {}
  ~CImpl() override {}

 private:
  void D() override {
    *d_called_ = true;
    closure_.Run();
  }

  bool* d_called_;
  StrongBinding<C> binding_;
  base::Closure closure_;
};

class BImpl : public B {
 public:
  BImpl(bool* d_called, InterfaceRequest<B> request,
        const base::Closure& closure)
      : d_called_(d_called), binding_(this, std::move(request)),
        closure_(closure) {}
  ~BImpl() override {}

 private:
  void GetC(InterfaceRequest<C> c) override {
    new CImpl(d_called_, std::move(c), closure_);
  }

  bool* d_called_;
  StrongBinding<B> binding_;
  base::Closure closure_;
};

class AImpl : public A {
 public:
  AImpl(InterfaceRequest<A> request, const base::Closure& closure)
      : d_called_(false), binding_(this, std::move(request)),
        closure_(closure) {}
  ~AImpl() override {}

  bool d_called() const { return d_called_; }

 private:
  void GetB(InterfaceRequest<B> b) override {
    new BImpl(&d_called_, std::move(b), closure_);
  }

  bool d_called_;
  Binding<A> binding_;
  base::Closure closure_;
};

TEST_F(InterfacePtrTest, Scoping) {
  APtr a;
  base::RunLoop run_loop;
  AImpl a_impl(GetProxy(&a), run_loop.QuitClosure());

  EXPECT_FALSE(a_impl.d_called());

  {
    BPtr b;
    a->GetB(GetProxy(&b));
    CPtr c;
    b->GetC(GetProxy(&c));
    c->D();
  }

  // While B & C have fallen out of scope, the pipes will remain until they are
  // flushed.
  EXPECT_FALSE(a_impl.d_called());
  run_loop.Run();
  EXPECT_TRUE(a_impl.d_called());
}

class PingTestImpl : public sample::PingTest {
 public:
  explicit PingTestImpl(InterfaceRequest<sample::PingTest> request)
      : binding_(this, std::move(request)) {}
  ~PingTestImpl() override {}

 private:
  // sample::PingTest:
  void Ping(const PingCallback& callback) override { callback.Run(); }

  Binding<sample::PingTest> binding_;
};

// Tests that FuseProxy does what it's supposed to do.
TEST_F(InterfacePtrTest, Fusion) {
  sample::PingTestPtr proxy;
  PingTestImpl impl(GetProxy(&proxy));

  // Create another PingTest pipe.
  sample::PingTestPtr ptr;
  sample::PingTestRequest request = GetProxy(&ptr);

  // Fuse the new pipe to the one hanging off |impl|.
  EXPECT_TRUE(FuseInterface(std::move(request), proxy.PassInterface()));

  // Ping!
  bool called = false;
  base::RunLoop loop;
  ptr->Ping([&called, &loop] {
    called = true;
    loop.Quit();
  });
  loop.Run();
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace test
}  // namespace mojo
