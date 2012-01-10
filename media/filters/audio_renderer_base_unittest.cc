// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/gtest_prod_util.h"
#include "base/stl_util.h"
#include "media/base/data_buffer.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/filters/audio_renderer_base.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

// Constants for distinguishing between muted audio and playing audio when using
// ConsumeBufferedData().
static uint8 kMutedAudio = 0x00;
static uint8 kPlayingAudio = 0x99;

// Mocked subclass of AudioRendererBase for testing purposes.
class MockAudioRendererBase : public AudioRendererBase {
 public:
  MockAudioRendererBase()
      : AudioRendererBase() {}
  virtual ~MockAudioRendererBase() {}

  // AudioRenderer implementation.
  MOCK_METHOD1(SetVolume, void(float volume));

  // AudioRendererBase implementation.
  MOCK_METHOD3(OnInitialize, bool(int, ChannelLayout, int));
  MOCK_METHOD0(OnStop, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererBase);
};

class AudioRendererBaseTest : public ::testing::Test {
 public:
  // Give the decoder some non-garbage media properties.
  AudioRendererBaseTest()
      : renderer_(new MockAudioRendererBase()),
        decoder_(new MockAudioDecoder()) {
    renderer_->set_host(&host_);

    // Queue all reads from the decoder by default.
    ON_CALL(*decoder_, Read(_))
        .WillByDefault(Invoke(this, &AudioRendererBaseTest::SaveReadCallback));

    // Set up audio properties.
    ON_CALL(*decoder_, bits_per_channel())
        .WillByDefault(Return(16));
    ON_CALL(*decoder_, channel_layout())
        .WillByDefault(Return(CHANNEL_LAYOUT_MONO));
    ON_CALL(*decoder_, samples_per_second())
        .WillByDefault(Return(44100));

    EXPECT_CALL(*decoder_, bits_per_channel())
        .Times(AnyNumber());
    EXPECT_CALL(*decoder_, channel_layout())
        .Times(AnyNumber());
    EXPECT_CALL(*decoder_, samples_per_second())
        .Times(AnyNumber());
  }

  virtual ~AudioRendererBaseTest() {
    EXPECT_CALL(*renderer_, OnStop());
    renderer_->Stop(NewExpectedClosure());
  }

  MOCK_METHOD1(OnSeekComplete, void(PipelineStatus));
  FilterStatusCB NewSeekCB() {
    return base::Bind(&AudioRendererBaseTest::OnSeekComplete,
                      base::Unretained(this));
  }

  MOCK_METHOD0(OnUnderflow, void());
  base::Closure NewUnderflowClosure() {
    return base::Bind(&AudioRendererBaseTest::OnUnderflow,
                      base::Unretained(this));
  }

  void Initialize() {
    EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
        .WillOnce(Return(true));
    renderer_->Initialize(
        decoder_, NewExpectedClosure(), NewUnderflowClosure());
  }

  void Preroll() {
    // Seek to trigger prerolling.
    EXPECT_CALL(*decoder_, Read(_));
    renderer_->Seek(base::TimeDelta(), NewSeekCB());

    // Fill entire buffer to complete prerolling.
    EXPECT_CALL(*this, OnSeekComplete(PIPELINE_OK));
    DeliverRemainingAudio();
  }

  void Play() {
    renderer_->Play(NewExpectedClosure());
    renderer_->SetPlaybackRate(1.0f);
  }

  // Delivers |size| bytes with value kPlayingAudio to |renderer_|.
  //
  // There must be a pending read callback.
  void FulfillPendingRead(size_t size) {
    CHECK(!read_cb_.is_null());
    scoped_refptr<DataBuffer> buffer(new DataBuffer(size));
    buffer->SetDataSize(size);
    memset(buffer->GetWritableData(), kPlayingAudio, buffer->GetDataSize());

    AudioDecoder::ReadCB read_cb;
    std::swap(read_cb, read_cb_);
    read_cb.Run(buffer);
  }

  // Delivers an end of stream buffer to |renderer_|.
  //
  // There must be a pending read callback.
  void DeliverEndOfStream() {
    FulfillPendingRead(0);
  }

  // Delivers bytes until |renderer_|'s internal buffer is full and no longer
  // has pending reads.
  void DeliverRemainingAudio() {
    CHECK(!read_cb_.is_null());
    FulfillPendingRead(bytes_remaining_in_buffer());
    CHECK(read_cb_.is_null());
  }

  // Attempts to consume |size| bytes from |renderer_|'s internal buffer,
  // returning true if all |size| bytes were consumed, false if less than
  // |size| bytes were consumed.
  //
  // |muted| is optional and if passed will get set if the byte value of
  // the consumed data is muted audio.
  bool ConsumeBufferedData(uint32 size, bool* muted) {
    scoped_array<uint8> buffer(new uint8[size]);
    uint32 bytes_read = renderer_->FillBuffer(buffer.get(), size,
                                              base::TimeDelta(), true);
    if (bytes_read > 0 && muted) {
      *muted = (buffer[0] == kMutedAudio);
    }
    return (bytes_read == size);
  }

  uint32 bytes_buffered() {
    return renderer_->algorithm_->bytes_buffered();
  }

  uint32 buffer_capacity() {
    return renderer_->algorithm_->QueueCapacity();
  }

  uint32 bytes_remaining_in_buffer() {
    // This can happen if too much data was delivered, in which case the buffer
    // will accept the data but not increase capacity.
    if (bytes_buffered() > buffer_capacity()) {
      return 0;
    }
    return buffer_capacity() - bytes_buffered();
  }

  void CallResumeAfterUnderflow() {
    renderer_->ResumeAfterUnderflow(false);
  }

  // Fixture members.
  scoped_refptr<MockAudioRendererBase> renderer_;
  scoped_refptr<MockAudioDecoder> decoder_;
  StrictMock<MockFilterHost> host_;
  AudioDecoder::ReadCB read_cb_;

 private:
  void SaveReadCallback(const AudioDecoder::ReadCB& callback) {
    CHECK(read_cb_.is_null()) << "Overlapping reads are not permitted";
    read_cb_ = callback;
  }

  DISALLOW_COPY_AND_ASSIGN(AudioRendererBaseTest);
};

TEST_F(AudioRendererBaseTest, Initialize_Failed) {
  EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_INITIALIZATION_FAILED));
  renderer_->Initialize(decoder_, NewExpectedClosure(), NewUnderflowClosure());

  // We should have no reads.
  EXPECT_TRUE(read_cb_.is_null());
}

TEST_F(AudioRendererBaseTest, Initialize_Successful) {
  EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
      .WillOnce(Return(true));
  renderer_->Initialize(decoder_, NewExpectedClosure(), NewUnderflowClosure());

  // We should have no reads.
  EXPECT_TRUE(read_cb_.is_null());
}

TEST_F(AudioRendererBaseTest, Preroll) {
  Initialize();
  Preroll();
}

TEST_F(AudioRendererBaseTest, Play) {
  Initialize();
  Preroll();
  Play();

  // Drain internal buffer, we should have a pending read.
  EXPECT_CALL(*decoder_, Read(_));
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered(), NULL));
}

TEST_F(AudioRendererBaseTest, EndOfStream) {
  Initialize();
  Preroll();
  Play();

  // Drain internal buffer, we should have a pending read.
  EXPECT_CALL(*decoder_, Read(_));
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered(), NULL));

  // Fulfill the read with an end-of-stream packet, we shouldn't report ended
  // nor have a read until we drain the internal buffer.
  DeliverEndOfStream();
  EXPECT_FALSE(renderer_->HasEnded());

  // Drain internal buffer, now we should report ended.
  EXPECT_CALL(host_, NotifyEnded());
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered(), NULL));
  EXPECT_TRUE(renderer_->HasEnded());
}

TEST_F(AudioRendererBaseTest, Underflow) {
  Initialize();
  Preroll();
  Play();

  // Drain internal buffer, we should have a pending read.
  EXPECT_CALL(*decoder_, Read(_));
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered(), NULL));

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  const size_t kDataSize = 1024;
  EXPECT_CALL(*this, OnUnderflow());
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));

  renderer_->ResumeAfterUnderflow(false);

  // Verify after resuming that we're still not getting data.
  //
  // NOTE: FillBuffer() satisfies the read but returns muted audio, which
  // is crazy http://crbug.com/106600
  bool muted = false;
  EXPECT_EQ(0u, bytes_buffered());
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_TRUE(muted);

  // Deliver data, we should get non-muted audio.
  DeliverRemainingAudio();
  EXPECT_CALL(*decoder_, Read(_));
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_FALSE(muted);
}

TEST_F(AudioRendererBaseTest, Underflow_EndOfStream) {
  Initialize();
  Preroll();
  Play();

  // Drain internal buffer, we should have a pending read.
  EXPECT_CALL(*decoder_, Read(_));
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered(), NULL));

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  const size_t kDataSize = 1024;
  EXPECT_CALL(*this, OnUnderflow());
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));

  // Deliver a little bit of data.
  EXPECT_CALL(*decoder_, Read(_));
  FulfillPendingRead(kDataSize);

  // Verify we're getting muted audio during underflow.
  //
  // NOTE: FillBuffer() satisfies the read but returns muted audio, which
  // is crazy http://crbug.com/106600
  bool muted = false;
  EXPECT_EQ(kDataSize, bytes_buffered());
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_TRUE(muted);

  // Now deliver end of stream, we should get our little bit of data back.
  DeliverEndOfStream();
  EXPECT_CALL(*decoder_, Read(_));
  EXPECT_EQ(kDataSize, bytes_buffered());
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_FALSE(muted);

  // Deliver another end of stream buffer and attempt to read to make sure
  // we're truly at the end of stream.
  //
  // TODO(scherkus): fix AudioRendererBase and AudioRendererAlgorithmBase to
  // stop reading after receiving an end of stream buffer. It should have also
  // called NotifyEnded() http://crbug.com/106641
  DeliverEndOfStream();
  EXPECT_CALL(host_, NotifyEnded());
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_FALSE(muted);
}

TEST_F(AudioRendererBaseTest, Underflow_ResumeFromCallback) {
  Initialize();
  Preroll();
  Play();

  // Drain internal buffer, we should have a pending read.
  EXPECT_CALL(*decoder_, Read(_));
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered(), NULL));

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  const size_t kDataSize = 1024;
  EXPECT_CALL(*this, OnUnderflow())
      .WillOnce(Invoke(this, &AudioRendererBaseTest::CallResumeAfterUnderflow));
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));

  // Verify after resuming that we're still not getting data.
  bool muted = false;
  EXPECT_EQ(0u, bytes_buffered());
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_TRUE(muted);

  // Deliver data, we should get non-muted audio.
  DeliverRemainingAudio();
  EXPECT_CALL(*decoder_, Read(_));
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_FALSE(muted);
}

}  // namespace media
