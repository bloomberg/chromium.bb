// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/bindings/tests/sample_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

const char kText1[] = "hello";
const char kText2[] = "world";

class SampleFactoryImpl : public InterfaceImpl<sample::Factory> {
 public:
  virtual void OnConnectionError() MOJO_OVERRIDE {
    delete this;
  }

  virtual void SetClient(sample::FactoryClient* client) MOJO_OVERRIDE {
    client_ = client;
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

  virtual void DoStuff2(ScopedDataPipeConsumerHandle pipe) MOJO_OVERRIDE {
    // Read the data from the pipe, writing the response (as a string) to
    // DidStuff2().
    ASSERT_TRUE(pipe.is_valid());
    uint32_t data_size = 0;
    ASSERT_EQ(MOJO_RESULT_OK,
              ReadDataRaw(pipe.get(), NULL, &data_size,
                          MOJO_READ_DATA_FLAG_QUERY));
    ASSERT_NE(0, static_cast<int>(data_size));
    char data[64];
    ASSERT_LT(static_cast<int>(data_size), 64);
    ASSERT_EQ(MOJO_RESULT_OK,
              ReadDataRaw(pipe.get(), data, &data_size,
                          MOJO_READ_DATA_FLAG_ALL_OR_NONE));

    AllocationScope scope;
    client_->DidStuff2(String(std::string(data)));
  }

 private:
  sample::FactoryClient* client_;
  ScopedMessagePipeHandle pipe1_;
};

class SampleFactoryClientImpl : public sample::FactoryClient {
 public:
  SampleFactoryClientImpl() : got_response_(false) {
  }

  void set_expected_text_reply(const std::string& expected_text_reply) {
    expected_text_reply_ = expected_text_reply;
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

  virtual void DidStuff2(const String& text_reply) MOJO_OVERRIDE {
    got_response_ = true;
    EXPECT_EQ(expected_text_reply_, text_reply.To<std::string>());
  }

 private:
  ScopedMessagePipeHandle pipe1_;
  ScopedMessagePipeHandle pipe3_;
  std::string expected_text_reply_;
  bool got_response_;
};

class HandlePassingTest : public testing::Test {
 public:
  virtual void TearDown() {
    PumpMessages();
  }

  void PumpMessages() {
    loop_.RunUntilIdle();
  }

 private:
  Environment env_;
  RunLoop loop_;
};

TEST_F(HandlePassingTest, Basic) {
  sample::FactoryPtr factory;
  BindToProxy(new SampleFactoryImpl(), &factory);

  SampleFactoryClientImpl factory_client;
  factory_client.set_expected_text_reply(kText1);

  factory->SetClient(&factory_client);

  ScopedMessagePipeHandle pipe0, pipe1;
  CreateMessagePipe(&pipe0, &pipe1);

  EXPECT_TRUE(WriteTextMessage(pipe1.get(), kText1));

  ScopedMessagePipeHandle pipe2, pipe3;
  CreateMessagePipe(&pipe2, &pipe3);

  EXPECT_TRUE(WriteTextMessage(pipe3.get(), kText2));

  {
    AllocationScope scope;
    sample::Request::Builder request;
    request.set_x(1);
    request.set_pipe(pipe2.Pass());
    factory->DoStuff(request.Finish(), pipe0.Pass());
  }

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

TEST_F(HandlePassingTest, PassInvalid) {
  sample::FactoryPtr factory;
  BindToProxy(new SampleFactoryImpl(), &factory);

  SampleFactoryClientImpl factory_client;
  factory->SetClient(&factory_client);

  {
    AllocationScope scope;
    sample::Request::Builder request;
    request.set_x(1);
    factory->DoStuff(request.Finish(), ScopedMessagePipeHandle().Pass());
  }

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

// Verifies DataPipeConsumer can be passed and read from.
TEST_F(HandlePassingTest, DataPipe) {
  sample::FactoryPtr factory;
  BindToProxy(new SampleFactoryImpl(), &factory);

  SampleFactoryClientImpl factory_client;
  factory->SetClient(&factory_client);

  // Writes a string to a data pipe and passes the data pipe (consumer) to the
  // factory.
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  MojoCreateDataPipeOptions options = {
      sizeof(MojoCreateDataPipeOptions),
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
      1,
      1024};
  ASSERT_EQ(MOJO_RESULT_OK,
            CreateDataPipe(&options, &producer_handle, &consumer_handle));
  std::string expected_text_reply = "got it";
  factory_client.set_expected_text_reply(expected_text_reply);
  // +1 for \0.
  uint32_t data_size = static_cast<uint32_t>(expected_text_reply.size() + 1);
  ASSERT_EQ(MOJO_RESULT_OK,
            WriteDataRaw(producer_handle.get(), expected_text_reply.c_str(),
                         &data_size, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));

  {
    AllocationScope scope;
    factory->DoStuff2(consumer_handle.Pass());
  }

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

TEST_F(HandlePassingTest, PipesAreClosed) {
  sample::FactoryPtr factory;
  BindToProxy(new SampleFactoryImpl(), &factory);

  SampleFactoryClientImpl factory_client;
  factory->SetClient(&factory_client);

  MessagePipe extra_pipe;

  MojoHandle handle0_value = extra_pipe.handle0.get().value();
  MojoHandle handle1_value = extra_pipe.handle1.get().value();

  {
    AllocationScope scope;

    Array<MessagePipeHandle>::Builder pipes(2);
    pipes[0] = extra_pipe.handle0.Pass();
    pipes[1] = extra_pipe.handle1.Pass();

    sample::Request::Builder request_builder;
    request_builder.set_more_pipes(pipes.Finish());

    sample::Request request = request_builder.Finish();

    factory->DoStuff(request, ScopedMessagePipeHandle());

    // The handles should have been transferred to the underlying Message.
    EXPECT_EQ(MOJO_HANDLE_INVALID, request.more_pipes()[0].get().value());
    EXPECT_EQ(MOJO_HANDLE_INVALID, request.more_pipes()[1].get().value());
  }

  // We expect the pipes to have been closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(handle0_value));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(handle1_value));
}

}  // namespace
}  // namespace test
}  // namespace mojo
