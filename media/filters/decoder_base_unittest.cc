// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_task.h"
#include "media/base/pipeline.h"
#include "media/filters/decoder_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NotNull;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace media {

class GTEST_TEST_CLASS_NAME_(DecoderBaseTest, FlowControl);

}  // namespace media

namespace {

class MockDecoderOutput : public media::StreamSample {
 public:
  MOCK_CONST_METHOD0(IsEndOfStream, bool());
};

class MockBuffer : public media::Buffer {
 public:
  MockBuffer() {}
  virtual ~MockBuffer() {}
  MOCK_CONST_METHOD0(GetData, const uint8*());
  MOCK_CONST_METHOD0(GetDataSize, size_t());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBuffer);
};

class MockDecoder : public media::Filter {
 public:
  typedef Callback1<scoped_refptr<MockDecoderOutput> >::Type
      ConsumeAudioSamplesCallback;
  void set_consume_audio_samples_callback(
      ConsumeAudioSamplesCallback* callback) {
    consume_audio_samples_callback_.reset(callback);
  }
  ConsumeAudioSamplesCallback* consume_audio_samples_callback() {
    return consume_audio_samples_callback_.get();
  }
  scoped_ptr<ConsumeAudioSamplesCallback> consume_audio_samples_callback_;
};

class MockDecoderCallback {
 public:
  MOCK_METHOD1(OnReadComplete, void(scoped_refptr<MockDecoderOutput> output));
};

class MockDecoderImpl : public media::DecoderBase<
  MockDecoder, MockDecoderOutput> {
 public:
  explicit MockDecoderImpl(MessageLoop* message_loop)
      : media::DecoderBase<MockDecoder, MockDecoderOutput>(message_loop) {
    media_format_.SetAsString(media::MediaFormat::kMimeType, "mock");
  }

  virtual ~MockDecoderImpl() {}

  // DecoderBase Implementations.
  MOCK_METHOD3(DoInitialize,
               void(media::DemuxerStream* demuxer_stream,
                    bool* success,
                    Task* done_cb));
  MOCK_METHOD1(DoStop, void(Task* done_cb));
  MOCK_METHOD2(DoSeek, void(base::TimeDelta time, Task* done_cb));
  MOCK_METHOD1(DoDecode, void(media::Buffer* input));

 private:
  FRIEND_TEST(media::DecoderBaseTest, FlowControl);

  DISALLOW_COPY_AND_ASSIGN(MockDecoderImpl);
};

}  // namespace

namespace media {

ACTION(Initialize) {
  AutoTaskRunner done_runner(arg2);
  *arg1 = true;
}

ACTION_P(SaveDecodeRequest, list) {
  scoped_refptr<Buffer> buffer(arg0);
  list->push_back(buffer);
}

ACTION(CompleteDemuxRequest) {
  arg0->Run(new MockBuffer());
  delete arg0;
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
      NewCallback(&read_callback, &MockDecoderCallback::OnReadComplete));
  scoped_refptr<MockDemuxerStream> demuxer_stream(new MockDemuxerStream());
  MockStatisticsCallback stats_callback_object;

  // Initialize.
  EXPECT_CALL(*decoder, DoInitialize(NotNull(), NotNull(), NotNull()))
      .WillOnce(Initialize());
  decoder->Initialize(demuxer_stream.get(), NewExpectedCallback(),
                      NewCallback(&stats_callback_object,
                                  &MockStatisticsCallback::OnStatistics));
  message_loop.RunAllPending();

  // Read.
  std::vector<scoped_refptr<Buffer> > decode_requests;
  EXPECT_CALL(*demuxer_stream, Read(NotNull()))
      .Times(2)
      .WillRepeatedly(CompleteDemuxRequest());
  EXPECT_CALL(*decoder, DoDecode(NotNull()))
      .Times(2)
      .WillRepeatedly(SaveDecodeRequest(&decode_requests));
  scoped_refptr<MockDecoderOutput> output;
  decoder->ProduceAudioSamples(output);
  decoder->ProduceAudioSamples(output);
  message_loop.RunAllPending();

  // Fulfill the decode request.
  EXPECT_CALL(read_callback, OnReadComplete(_)).Times(2);
  PipelineStatistics statistics;
  for (size_t i = 0; i < decode_requests.size(); ++i) {
    decoder->EnqueueResult(new MockDecoderOutput());
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
