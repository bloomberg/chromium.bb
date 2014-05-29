// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  virtual void DoStuff(sample::RequestPtr request,
                       ScopedMessagePipeHandle pipe) MOJO_OVERRIDE {
    std::string text1;
    if (pipe.is_valid())
      EXPECT_TRUE(ReadTextMessage(pipe.get(), &text1));

    std::string text2;
    if (request->pipe.is_valid()) {
      EXPECT_TRUE(ReadTextMessage(request->pipe.get(), &text2));

      // Ensure that simply accessing request->pipe does not close it.
      EXPECT_TRUE(request->pipe.is_valid());
    }

    ScopedMessagePipeHandle pipe0;
    if (!text2.empty()) {
      CreateMessagePipe(&pipe0, &pipe1_);
      EXPECT_TRUE(WriteTextMessage(pipe1_.get(), text2));
    }

    sample::ResponsePtr response(sample::Response::New());
    response->x = 2;
    response->pipe = pipe0.Pass();
    client()->DidStuff(response.Pass(), text1);
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

    client()->DidStuff2(data);
  }

 private:
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

  virtual void DidStuff(sample::ResponsePtr response,
                        const String& text_reply) MOJO_OVERRIDE {
    EXPECT_EQ(expected_text_reply_, text_reply);

    if (response->pipe.is_valid()) {
      std::string text2;
      EXPECT_TRUE(ReadTextMessage(response->pipe.get(), &text2));

      // Ensure that simply accessing response.pipe does not close it.
      EXPECT_TRUE(response->pipe.is_valid());

      EXPECT_EQ(std::string(kText2), text2);

      // Do some more tests of handle passing:
      ScopedMessagePipeHandle p = response->pipe.Pass();
      EXPECT_TRUE(p.is_valid());
      EXPECT_FALSE(response->pipe.is_valid());
    }

    got_response_ = true;
  }

  virtual void DidStuff2(const String& text_reply) MOJO_OVERRIDE {
    got_response_ = true;
    EXPECT_EQ(expected_text_reply_, text_reply);
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

  factory.set_client(&factory_client);

  ScopedMessagePipeHandle pipe0, pipe1;
  CreateMessagePipe(&pipe0, &pipe1);

  EXPECT_TRUE(WriteTextMessage(pipe1.get(), kText1));

  ScopedMessagePipeHandle pipe2, pipe3;
  CreateMessagePipe(&pipe2, &pipe3);

  EXPECT_TRUE(WriteTextMessage(pipe3.get(), kText2));

  sample::RequestPtr request(sample::Request::New());
  request->x = 1;
  request->pipe = pipe2.Pass();
  factory->DoStuff(request.Pass(), pipe0.Pass());

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

TEST_F(HandlePassingTest, PassInvalid) {
  sample::FactoryPtr factory;
  BindToProxy(new SampleFactoryImpl(), &factory);

  SampleFactoryClientImpl factory_client;
  factory.set_client(&factory_client);

  sample::RequestPtr request(sample::Request::New());
  request->x = 1;
  factory->DoStuff(request.Pass(), ScopedMessagePipeHandle().Pass());

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

// Verifies DataPipeConsumer can be passed and read from.
TEST_F(HandlePassingTest, DataPipe) {
  sample::FactoryPtr factory;
  BindToProxy(new SampleFactoryImpl(), &factory);

  SampleFactoryClientImpl factory_client;
  factory.set_client(&factory_client);

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

  factory->DoStuff2(consumer_handle.Pass());

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

TEST_F(HandlePassingTest, PipesAreClosed) {
  sample::FactoryPtr factory;
  BindToProxy(new SampleFactoryImpl(), &factory);

  SampleFactoryClientImpl factory_client;
  factory.set_client(&factory_client);

  MessagePipe extra_pipe;

  MojoHandle handle0_value = extra_pipe.handle0.get().value();
  MojoHandle handle1_value = extra_pipe.handle1.get().value();

  {
    Array<ScopedMessagePipeHandle> pipes(2);
    pipes[0] = extra_pipe.handle0.Pass();
    pipes[1] = extra_pipe.handle1.Pass();

    sample::RequestPtr request(sample::Request::New());
    request->more_pipes = pipes.Pass();

    factory->DoStuff(request.Pass(), ScopedMessagePipeHandle());
  }

  // We expect the pipes to have been closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(handle0_value));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(handle1_value));
}

TEST_F(HandlePassingTest, IsHandle) {
  // Validate that mojo::internal::IsHandle<> works as expected since this.
  // template is key to ensuring that we don't leak handles.
  EXPECT_TRUE(internal::IsHandle<Handle>::value);
  EXPECT_TRUE(internal::IsHandle<MessagePipeHandle>::value);
  EXPECT_TRUE(internal::IsHandle<DataPipeConsumerHandle>::value);
  EXPECT_TRUE(internal::IsHandle<DataPipeProducerHandle>::value);
  EXPECT_TRUE(internal::IsHandle<SharedBufferHandle>::value);

  // Basic sanity checks...
  EXPECT_FALSE(internal::IsHandle<int>::value);
  EXPECT_FALSE(internal::IsHandle<sample::FactoryPtr>::value);
  EXPECT_FALSE(internal::IsHandle<String>::value);
}

}  // namespace
}  // namespace test
}  // namespace mojo
