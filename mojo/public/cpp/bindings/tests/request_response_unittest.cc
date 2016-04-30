// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/interfaces/bindings/tests/sample_import.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class ProviderImpl : public sample::Provider {
 public:
  explicit ProviderImpl(InterfaceRequest<sample::Provider> request)
      : binding_(this, std::move(request)) {}

  void EchoString(const String& a,
                  const Callback<void(String)>& callback) override {
    Callback<void(String)> callback_copy;
    // Make sure operator= is used.
    callback_copy = callback;
    callback_copy.Run(a);
  }

  void EchoStrings(const String& a,
                   const String& b,
                   const Callback<void(String, String)>& callback) override {
    callback.Run(a, b);
  }

  void EchoMessagePipeHandle(
      ScopedMessagePipeHandle a,
      const Callback<void(ScopedMessagePipeHandle)>& callback) override {
    callback.Run(std::move(a));
  }

  void EchoEnum(sample::Enum a,
                const Callback<void(sample::Enum)>& callback) override {
    callback.Run(a);
  }

  void EchoInt(int32_t a, const EchoIntCallback& callback) override {
    callback.Run(a);
  }

  Binding<sample::Provider> binding_;
};

class StringRecorder {
 public:
  StringRecorder(std::string* buf, const base::Closure& closure)
      : buf_(buf), closure_(closure) {}
  void Run(const String& a) const {
    *buf_ = a;
    closure_.Run();
  }
  void Run(const String& a, const String& b) const {
    *buf_ = a.get() + b.get();
    closure_.Run();
  }

 private:
  std::string* buf_;
  base::Closure closure_;
};

class EnumRecorder {
 public:
  explicit EnumRecorder(sample::Enum* value, const base::Closure& closure)
      : value_(value), closure_(closure) {}
  void Run(sample::Enum a) const {
    *value_ = a;
    closure_.Run();
  }

 private:
  sample::Enum* value_;
  base::Closure closure_;
};

class MessagePipeWriter {
 public:
  MessagePipeWriter(const char* text, const base::Closure& closure)
      : text_(text), closure_(closure) {}
  void Run(ScopedMessagePipeHandle handle) const {
    WriteTextMessage(handle.get(), text_);
    closure_.Run();
  }

 private:
  std::string text_;
  base::Closure closure_;
};

class RequestResponseTest : public testing::Test {
 public:
  RequestResponseTest() : loop_(common::MessagePumpMojo::Create()) {}
  ~RequestResponseTest() override { loop_.RunUntilIdle(); }

  void PumpMessages() { loop_.RunUntilIdle(); }

 private:
  base::MessageLoop loop_;
};

TEST_F(RequestResponseTest, EchoString) {
  sample::ProviderPtr provider;
  ProviderImpl provider_impl(GetProxy(&provider));

  std::string buf;
  base::RunLoop run_loop;
  provider->EchoString(String::From("hello"),
                       StringRecorder(&buf, run_loop.QuitClosure()));

  run_loop.Run();

  EXPECT_EQ(std::string("hello"), buf);
}

TEST_F(RequestResponseTest, EchoStrings) {
  sample::ProviderPtr provider;
  ProviderImpl provider_impl(GetProxy(&provider));

  std::string buf;
  base::RunLoop run_loop;
  provider->EchoStrings(
      String::From("hello"), String::From(" world"),
      StringRecorder(&buf, run_loop.QuitClosure()));

  run_loop.Run();

  EXPECT_EQ(std::string("hello world"), buf);
}

TEST_F(RequestResponseTest, EchoMessagePipeHandle) {
  sample::ProviderPtr provider;
  ProviderImpl provider_impl(GetProxy(&provider));

  MessagePipe pipe2;
  base::RunLoop run_loop;
  provider->EchoMessagePipeHandle(
      std::move(pipe2.handle1),
      MessagePipeWriter("hello", run_loop.QuitClosure()));

  run_loop.Run();

  std::string value;
  ReadTextMessage(pipe2.handle0.get(), &value);

  EXPECT_EQ(std::string("hello"), value);
}

TEST_F(RequestResponseTest, EchoEnum) {
  sample::ProviderPtr provider;
  ProviderImpl provider_impl(GetProxy(&provider));

  sample::Enum value;
  base::RunLoop run_loop;
  provider->EchoEnum(sample::Enum::VALUE,
                     EnumRecorder(&value, run_loop.QuitClosure()));

  run_loop.Run();

  EXPECT_EQ(sample::Enum::VALUE, value);
}

}  // namespace
}  // namespace test
}  // namespace mojo
