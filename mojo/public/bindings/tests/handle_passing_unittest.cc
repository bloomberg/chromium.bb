// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/environment/environment.h"
#include "mojo/public/tests/test_support.h"
#include "mojo/public/utility/run_loop.h"
#include "mojom/sample_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

const char kText1[] = "hello";
const char kText2[] = "world";

class SampleFactoryImpl : public sample::Factory {
 public:
  explicit SampleFactoryImpl(sample::ScopedFactoryClientHandle handle)
      : client_(handle.Pass(), this) {
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
  RemotePtr<sample::FactoryClient> client_;
  ScopedMessagePipeHandle pipe1_;
};

class SampleFactoryClientImpl : public sample::FactoryClient {
 public:
  explicit SampleFactoryClientImpl(sample::ScopedFactoryHandle handle)
      : factory_(handle.Pass(), this),
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

  // Writes a string to a data pipe and passes the data pipe (consumer) to the
  // factory.
  void StartDataPipe() {
    expected_text_reply_.clear();

    ScopedDataPipeProducerHandle producer_handle;
    ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options = {
        sizeof(MojoCreateDataPipeOptions),
        MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
        1,
        1024};
    ASSERT_EQ(MOJO_RESULT_OK,
              CreateDataPipe(&options, &producer_handle, &consumer_handle));
    expected_text_reply_ = "got it";
    // +1 for \0.
    uint32_t data_size = static_cast<uint32_t>(expected_text_reply_.size() + 1);
    ASSERT_EQ(MOJO_RESULT_OK,
              WriteDataRaw(producer_handle.get(), expected_text_reply_.c_str(),
                           &data_size, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));

    AllocationScope scope;
    factory_->DoStuff2(consumer_handle.Pass());
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
  RemotePtr<sample::Factory> factory_;
  ScopedMessagePipeHandle pipe1_;
  ScopedMessagePipeHandle pipe3_;
  std::string expected_text_reply_;
  bool got_response_;
};

}  // namespace

class HandlePassingTest : public testing::Test {
 public:
  void PumpMessages() {
    loop_.RunUntilIdle();
  }

 private:
  Environment env_;
  RunLoop loop_;
};

TEST_F(HandlePassingTest, Basic) {
  InterfacePipe<sample::Factory> pipe;

  SampleFactoryImpl factory(pipe.handle_to_peer.Pass());
  SampleFactoryClientImpl factory_client(pipe.handle_to_self.Pass());

  factory_client.Start();

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

TEST_F(HandlePassingTest, PassInvalid) {
  InterfacePipe<sample::Factory> pipe;

  SampleFactoryImpl factory(pipe.handle_to_peer.Pass());
  SampleFactoryClientImpl factory_client(pipe.handle_to_self.Pass());

  factory_client.StartNoPipes();

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

// Verifies DataPipeConsumer can be passed and read from.
TEST_F(HandlePassingTest, DataPipe) {
  InterfacePipe<sample::Factory> pipe;

  SampleFactoryImpl factory(pipe.handle_to_peer.Pass());
  SampleFactoryClientImpl factory_client(pipe.handle_to_self.Pass());

  ASSERT_NO_FATAL_FAILURE(factory_client.StartDataPipe());

  EXPECT_FALSE(factory_client.got_response());

  PumpMessages();

  EXPECT_TRUE(factory_client.got_response());
}

}  // namespace test
}  // namespace mojo
