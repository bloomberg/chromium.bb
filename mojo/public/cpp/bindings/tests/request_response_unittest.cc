// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/bindings/tests/sample_import.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class ProviderImpl : public sample::Provider {
 public:
  explicit ProviderImpl(sample::ScopedProviderClientHandle handle)
      : client_(handle.Pass(), this) {
  }

  virtual void EchoString(
      const String& a,
      const Callback<void(String)>& callback) MOJO_OVERRIDE {
    AllocationScope scope;
    Callback<void(String)> callback_copy;
    // Make sure operator= is used.
    callback_copy = callback;
    callback_copy.Run(a);
  }

  virtual void EchoStrings(
      const String& a,
      const String& b,
      const Callback<void(String, String)>& callback) MOJO_OVERRIDE {
    AllocationScope scope;
    callback.Run(a, b);
  }

  virtual void EchoMessagePipeHandle(
      ScopedMessagePipeHandle a,
      const Callback<void(ScopedMessagePipeHandle)>& callback) MOJO_OVERRIDE {
    AllocationScope scope;
    callback.Run(a.Pass());
  }

  virtual void EchoEnum(sample::Enum a,
                        const Callback<void(sample::Enum)>& callback)
      MOJO_OVERRIDE {
    callback.Run(a);
  }

 private:
  RemotePtr<sample::ProviderClient> client_;
};

class StringRecorder {
 public:
  StringRecorder(std::string* buf) : buf_(buf) {
  }
  void Run(const String& a) const {
    *buf_ = a.To<std::string>();
  }
  void Run(const String& a, const String& b) const {
    *buf_ = a.To<std::string>() + b.To<std::string>();
  }
 private:
  std::string* buf_;
};

class EnumRecorder {
 public:
  EnumRecorder(sample::Enum* value) : value_(value) {
  }
  void Run(sample::Enum a) const {
    *value_ = a;
  }
 private:
  sample::Enum* value_;
};

class MessagePipeWriter {
 public:
  explicit MessagePipeWriter(const char* text) : text_(text) {
  }
  void Run(ScopedMessagePipeHandle handle) const {
    WriteTextMessage(handle.get(), text_);
  }
 private:
  std::string text_;
};

class RequestResponseTest : public testing::Test {
 public:
  void PumpMessages() {
    loop_.RunUntilIdle();
  }

 private:
  Environment env_;
  RunLoop loop_;
};

TEST_F(RequestResponseTest, EchoString) {
  InterfacePipe<sample::Provider> pipe;
  ProviderImpl provider_impl(pipe.handle_to_peer.Pass());
  RemotePtr<sample::Provider> provider(pipe.handle_to_self.Pass(), NULL);

  std::string buf;
  {
    AllocationScope scope;
    provider->EchoString("hello", StringRecorder(&buf));
  }

  PumpMessages();

  EXPECT_EQ(std::string("hello"), buf);
}

TEST_F(RequestResponseTest, EchoStrings) {
  InterfacePipe<sample::Provider> pipe;
  ProviderImpl provider_impl(pipe.handle_to_peer.Pass());
  RemotePtr<sample::Provider> provider(pipe.handle_to_self.Pass(), NULL);

  std::string buf;
  {
    AllocationScope scope;
    provider->EchoStrings("hello", " world", StringRecorder(&buf));
  }

  PumpMessages();

  EXPECT_EQ(std::string("hello world"), buf);
}

TEST_F(RequestResponseTest, EchoMessagePipeHandle) {
  InterfacePipe<sample::Provider> pipe;
  ProviderImpl provider_impl(pipe.handle_to_peer.Pass());
  RemotePtr<sample::Provider> provider(pipe.handle_to_self.Pass(), NULL);

  MessagePipe pipe2;
  {
    AllocationScope scope;
    provider->EchoMessagePipeHandle(pipe2.handle1.Pass(),
                                    MessagePipeWriter("hello"));
  }

  PumpMessages();

  std::string value;
  ReadTextMessage(pipe2.handle0.get(), &value);

  EXPECT_EQ(std::string("hello"), value);
}

TEST_F(RequestResponseTest, EchoEnum) {
  InterfacePipe<sample::Provider> pipe;
  ProviderImpl provider_impl(pipe.handle_to_peer.Pass());
  RemotePtr<sample::Provider> provider(pipe.handle_to_self.Pass(), NULL);

  sample::Enum value;
  {
    AllocationScope scope;
    provider->EchoEnum(sample::ENUM_VALUE, EnumRecorder(&value));
  }

  PumpMessages();

  EXPECT_EQ(sample::ENUM_VALUE, value);
}

}  // namespace
}  // namespace test
}  // namespace mojo
