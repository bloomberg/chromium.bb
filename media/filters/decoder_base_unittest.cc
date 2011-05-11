// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "media/base/data_buffer.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_task.h"
#include "media/base/pipeline.h"
#include "media/filters/decoder_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NotNull;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace media {

class GTEST_TEST_CLASS_NAME_(DecoderBaseTest, FlowControl);

class MockDecoderCallback {
 public:
  MOCK_METHOD1(OnReadComplete, void(scoped_refptr<Buffer> output));
};

class MockDecoderImpl : public DecoderBase<MockAudioDecoder, Buffer> {
 public:
  explicit MockDecoderImpl(MessageLoop* message_loop)
      : DecoderBase<MockAudioDecoder, Buffer>(message_loop) {
  }

  virtual ~MockDecoderImpl() {}

  // DecoderBase implementation.
  MOCK_METHOD3(DoInitialize,
               void(DemuxerStream* demuxer_stream,
                    bool* success,
                    Task* done_cb));
  MOCK_METHOD1(DoStop, void(Task* done_cb));
  MOCK_METHOD2(DoSeek, void(base::TimeDelta time, Task* done_cb));
  MOCK_METHOD1(DoDecode, void(Buffer* input));

 private:
  FRIEND_TEST(DecoderBaseTest, FlowControl);

  DISALLOW_COPY_AND_ASSIGN(MockDecoderImpl);
};

ACTION(Initialize) {
  AutoTaskRunner done_runner(arg2);
  *arg1 = true;
}

ACTION_P(SaveDecodeRequest, list) {
  scoped_refptr<Buffer> buffer(arg0);
  list->push_back(buffer);
}

ACTION(CompleteDemuxRequest) {
  arg0.Run(new DataBuffer(0));
}

// Test the flow control of decoder base by the following sequence of actions:
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
TEST(DecoderBaseTest, FlowControl) {
  MessageLoop message_loop;
  scoped_refptr<MockDecoderImpl> decoder(new MockDecoderImpl(&message_loop));

  MockDecoderCallback read_callback;
  decoder->set_consume_audio_samples_callback(
      base::Bind(&MockDecoderCallback::OnReadComplete,
                 base::Unretained(&read_callback)));

  scoped_refptr<MockDemuxerStream> demuxer_stream(new MockDemuxerStream());
  MockStatisticsCallback stats_callback_object;
  EXPECT_CALL(stats_callback_object, OnStatistics(_))
      .Times(AnyNumber());

  // Initialize.
  EXPECT_CALL(*decoder, DoInitialize(NotNull(), NotNull(), NotNull()))
      .WillOnce(Initialize());
  decoder->Initialize(demuxer_stream.get(), NewExpectedCallback(),
                      NewCallback(&stats_callback_object,
                                  &MockStatisticsCallback::OnStatistics));
  message_loop.RunAllPending();

  // Read.
  std::vector<scoped_refptr<Buffer> > decode_requests;
  EXPECT_CALL(*demuxer_stream, Read(_))
      .Times(2)
      .WillRepeatedly(CompleteDemuxRequest());
  EXPECT_CALL(*decoder, DoDecode(NotNull()))
      .Times(2)
      .WillRepeatedly(SaveDecodeRequest(&decode_requests));
  scoped_refptr<DataBuffer> output;
  decoder->PostReadTaskHack(output);
  decoder->PostReadTaskHack(output);
  message_loop.RunAllPending();

  // Fulfill the decode request.
  EXPECT_CALL(read_callback, OnReadComplete(_)).Times(2);
  PipelineStatistics statistics;
  for (size_t i = 0; i < decode_requests.size(); ++i) {
    decoder->EnqueueResult(new DataBuffer(0));
    decoder->OnDecodeComplete(statistics);
  }
  decode_requests.clear();
  message_loop.RunAllPending();

  // Stop.
  EXPECT_CALL(*decoder, DoStop(_))
      .WillOnce(WithArg<0>(InvokeRunnable()));
  decoder->Stop(NewExpectedCallback());
  message_loop.RunAllPending();
}

}  // namespace media
