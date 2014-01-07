// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojo/public/tests/simple_bindings_support.h"
#include "mojo/public/tests/test_support.h"
#include "mojom/sample_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

const char kText1[] = "hello";
const char kText2[] = "world";

class SampleFactoryImpl : public sample::Factory {
 public:
  explicit SampleFactoryImpl(ScopedMessagePipeHandle pipe)
      : client_(pipe.Pass(), this) {
  }

  virtual void DoStuff(const sample::Request& request,
                       ScopedMessagePipeHandle pipe) MOJO_OVERRIDE {
    std::string text1;
    if (pipe.is_valid())
      EXPECT_TRUE(ReadTextMessage(pipe.get(), &text1));

    std::string text2;
    if (request.pipe().is_valid()) {
      EXPECT_TRUE(ReadTextMessage(request.pipe().get(), &text2));

      // Ensure that simply accessing request.pipe() does not close it.
      EXPECT_TRUE(request.pipe().is_valid());
    }

    ScopedMessagePipeHandle pipe0;
    if (!text2.empty()) {
      CreateMessagePipe(&pipe0, &pipe1_);
      EXPECT_TRUE(WriteTextMessage(pipe1_.get(), text2));
    }

    AllocationScope scope;
    sample::Response::Builder response;
    response.set_x(2);
    response.set_pipe(pipe0.Pass());
    client_->DidStuff(response.Finish(), text1);
  }

 private:
  RemotePtr<sample::FactoryClient> client_;
  ScopedMessagePipeHandle pipe1_;
};

class SampleFactoryClientImpl : public sample::FactoryClient {
 public:
  explicit SampleFactoryClientImpl(ScopedMessagePipeHandle pipe)
      : factory_(pipe.Pass(), this),
        got_response_(false) {
  }

  void Start() {
    expected_text_reply_ = kText1;

    ScopedMessagePipeHandle pipe0;
    CreateMessagePipe(&pipe0, &pipe1_);

    EXPECT_TRUE(WriteTextMessage(pipe1_.get(), kText1));

    ScopedMessagePipeHandle pipe2;
    CreateMessagePipe(&pipe2, &pipe3_);

    EXPECT_TRUE(WriteTextMessage(pipe3_.get(), kText2));

    AllocationScope scope;
    sample::Request::Builder request;
    request.set_x(1);
    request.set_pipe(pipe2.Pass());
    factory_->DoStuff(request.Finish(), pipe0.Pass());
  }

  void StartNoPipes() {
    expected_text_reply_.clear();

    AllocationScope scope;
    sample::Request::Builder request;
    request.set_x(1);
    factory_->DoStuff(request.Finish(), ScopedMessagePipeHandle().Pass());
  }

  bool got_response() const {
    return got_response_;
  }

  virtual void DidStuff(const sample::Response& response,
                        const String& text_reply) MOJO_OVERRIDE {
    EXPECT_EQ(expected_text_reply_, text_reply.To<std::string>());

    if (response.pipe().is_valid()) {
      std::string text2;
      EXPECT_TRUE(ReadTextMessage(response.pipe().get(), &text2));

      // Ensure that simply accessing response.pipe() does not close it.
      EXPECT_TRUE(response.pipe().is_valid());

      EXPECT_EQ(std::string(kText2), text2);

      // Do some more tests of handle passing:
      ScopedMessagePipeHandle p = response.pipe().Pass();
      EXPECT_TRUE(p.is_valid());
      EXPECT_FALSE(response.pipe().is_valid());
    }

    got_response_ = true;
  }

 private:
  RemotePtr<sample::Factory> factory_;
  ScopedMessagePipeHandle pipe1_;
  ScopedMessagePipeHandle pipe3_;
  std::string expected_text_reply_;
  bool got_response_;
};

}  // namespace

class BindingsHandlePassingTest : public testing::Test {
 public:
  void PumpMessages() {
    bindings_support_.Process();
  }

 private:
  SimpleBindingsSupport bindings_support_;
};

TEST_F(BindingsHandlePassingTest, Basic) {
  ScopedMessagePipeHandle pipe0;
  ScopedMessagePipeHandle pipe1;
  CreateMessagePipe(&pipe0, &pipe1);

  SampleFactoryImpl factory(pipe0.Pass());
  SampleFactoryClientImpl factory_client(pipe1.Pass());

  factory_client.Start();

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

TEST_F(BindingsHandlePassingTest, PassInvalid) {
  ScopedMessagePipeHandle pipe0;
  ScopedMessagePipeHandle pipe1;
  CreateMessagePipe(&pipe0, &pipe1);

  SampleFactoryImpl factory(pipe0.Pass());
  SampleFactoryClientImpl factory_client(pipe1.Pass());

  factory_client.StartNoPipes();

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

}  // namespace test
}  // namespace mojo
