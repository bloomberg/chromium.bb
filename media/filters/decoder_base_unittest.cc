// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "media/base/mock_filters.h"
#include "media/filters/decoder_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NotNull;

namespace media {

class MockOutput : public StreamSample {
  MOCK_CONST_METHOD0(IsEndOfStream, bool());
};

class MockDecoder : public MediaFilter {};

class MockBuffer : public Buffer {
 public:
  MOCK_CONST_METHOD0(GetData, const uint8*());
  MOCK_CONST_METHOD0(GetDataSize, size_t());
};

ACTION(Initialize) {
  AutoTaskRunner done_runner(arg2);
  *arg1 = true;
}

ACTION_P(SaveDecodeRequest, list) {
  scoped_refptr<Buffer> buffer = arg0;
  list->push_back(arg1);
}

ACTION(CompleteDemuxRequest) {
  arg0->Run(new MockBuffer());
  delete arg0;
}

class MockDecoderImpl : public DecoderBase<MockDecoder, MockOutput> {
 public:
  MockDecoderImpl() {
    media_format_.SetAsString(MediaFormat::kMimeType, "mock");
  }

  // DecoderBase Implementations.
  MOCK_METHOD3(DoInitialize,
               void(DemuxerStream* demuxer_stream, bool* success,
                    Task* done_cb));
  MOCK_METHOD0(DoStop, void());
  MOCK_METHOD2(DoSeek, void(base::TimeDelta time, Task* done_cb));
  MOCK_METHOD2(DoDecode, void(Buffer* input, Task* done_cb));

 private:
  FRIEND_TEST(DecoderBaseTest, DISABLED_FlowControl);

  DISALLOW_COPY_AND_ASSIGN(MockDecoderImpl);
};

class MockReadCallback {
 public:
  MockReadCallback() {}
  MOCK_METHOD1(ReadCallback, void(MockOutput* output));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockReadCallback);
};

// Test the flow control of decoder base by the following sequnce of actions:
// - Read() -> DecoderStream
//   \ Read() -> DemuxerStream
// - Read() -> DecoderBase
//   \ Read() -> DemixerStream
// - ReadCallback() -> DecoderBase
//   \ DoDecode() -> Decoder
// - ReadCallback() -> DecoderBase
//   \ DoDecode() -> Decoder
// - DecodeCallback() -> DecoderBase
//   \ ReadCallback() -> client
// - DecodeCallback() -> DecoderBase
//   \ ReadCallback() -> client
TEST(DecoderBaseTest, DISABLED_FlowControl) {
  MessageLoop message_loop;
  scoped_refptr<MockDecoderImpl> decoder = new MockDecoderImpl();
  scoped_refptr<MockDemuxerStream> demuxer_stream = new MockDemuxerStream();
  MockFilterCallback callback;
  MockReadCallback read_callback;
  decoder->set_message_loop(&message_loop);

  // Initailize.
  EXPECT_CALL(callback, OnFilterCallback());
  EXPECT_CALL(callback, OnCallbackDestroyed());
  EXPECT_CALL(*decoder, DoInitialize(NotNull(), NotNull(), NotNull()))
      .WillOnce(Initialize());
  decoder->Initialize(demuxer_stream.get(), callback.NewCallback());
  message_loop.RunAllPending();

  // Read.
  std::vector<Task*> decode_requests;
  EXPECT_CALL(*demuxer_stream, Read(NotNull()))
      .Times(2)
      .WillRepeatedly(CompleteDemuxRequest());
  EXPECT_CALL(*decoder, DoDecode(NotNull(), NotNull()))
      .Times(2)
      .WillRepeatedly(SaveDecodeRequest(&decode_requests));
  decoder->Read(NewCallback(&read_callback,
                            &MockReadCallback::ReadCallback));
  decoder->Read(NewCallback(&read_callback,
                            &MockReadCallback::ReadCallback));
  message_loop.RunAllPending();

  // Fulfill the decode request.
  EXPECT_CALL(read_callback, ReadCallback(NotNull())).Times(2);
  for (size_t i = 0; i < decode_requests.size(); ++i) {
    decoder->EnqueueResult(new MockOutput());
    AutoTaskRunner done_cb(decode_requests[i]);
  }
  decode_requests.clear();
  message_loop.RunAllPending();

  // Stop.
  EXPECT_CALL(*decoder, DoStop());
  decoder->Stop();
  message_loop.RunAllPending();
}

}  // namespace media
