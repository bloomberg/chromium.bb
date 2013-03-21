// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/data_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/audio_renderer_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Time;
using ::base::TimeDelta;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::NiceMock;
using ::testing::StrictMock;

namespace media {

// Constants for distinguishing between muted audio and playing audio when using
// ConsumeBufferedData().
static uint8 kMutedAudio = 0x00;
static uint8 kPlayingAudio = 0x99;

class AudioRendererImplTest : public ::testing::Test {
 public:
  // Give the decoder some non-garbage media properties.
  AudioRendererImplTest()
      : demuxer_stream_(new MockDemuxerStream()),
        decoder_(new MockAudioDecoder()),
        audio_config_(kCodecVorbis, kSampleFormatPlanarF32,
                      CHANNEL_LAYOUT_STEREO, 44100, NULL, 0, false) {
    EXPECT_CALL(*demuxer_stream_, type())
        .WillRepeatedly(Return(DemuxerStream::AUDIO));
    EXPECT_CALL(*demuxer_stream_, audio_decoder_config())
        .WillRepeatedly(ReturnRef(audio_config_));

    // Used to save callbacks and run them at a later time.
    EXPECT_CALL(*decoder_, Read(_))
        .WillRepeatedly(Invoke(this, &AudioRendererImplTest::ReadDecoder));

    // Set up audio properties.
    EXPECT_CALL(*decoder_, bits_per_channel())
        .WillRepeatedly(Return(audio_config_.bits_per_channel()));
    EXPECT_CALL(*decoder_, channel_layout())
        .WillRepeatedly(Return(CHANNEL_LAYOUT_MONO));
    EXPECT_CALL(*decoder_, samples_per_second())
        .WillRepeatedly(Return(audio_config_.samples_per_second()));

    ScopedVector<AudioDecoder> decoders;
    decoders.push_back(decoder_);

    renderer_.reset(new AudioRendererImpl(
        message_loop_.message_loop_proxy(),
        new NiceMock<MockAudioRendererSink>(),
        decoders.Pass(),
        SetDecryptorReadyCB()));

    // Stub out time.
    renderer_->set_now_cb_for_testing(base::Bind(
        &AudioRendererImplTest::GetTime, base::Unretained(this)));
  }

  virtual ~AudioRendererImplTest() {
    SCOPED_TRACE("~AudioRendererImplTest()");
    WaitableMessageLoopEvent event;
    renderer_->Stop(event.GetClosure());
    event.RunAndWait();
  }

  void ExpectUnsupportedAudioDecoder() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(RunCallback<1>(DECODER_ERROR_NOT_SUPPORTED));
  }

  void ExpectUnsupportedAudioDecoderConfig() {
    EXPECT_CALL(*decoder_, bits_per_channel())
        .WillRepeatedly(Return(3));
    EXPECT_CALL(*decoder_, channel_layout())
        .WillRepeatedly(Return(CHANNEL_LAYOUT_UNSUPPORTED));
    EXPECT_CALL(*decoder_, samples_per_second())
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));
  }

  MOCK_METHOD1(OnStatistics, void(const PipelineStatistics&));
  MOCK_METHOD0(OnUnderflow, void());
  MOCK_METHOD0(OnDisabled, void());
  MOCK_METHOD1(OnError, void(PipelineStatus));

  void OnAudioTimeCallback(TimeDelta current_time, TimeDelta max_time) {
    CHECK(current_time <= max_time);
  }

  void Initialize() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));

    InitializeWithStatus(PIPELINE_OK);

    int channels = ChannelLayoutToChannelCount(decoder_->channel_layout());
    int bytes_per_frame = decoder_->bits_per_channel() * channels / 8;
    next_timestamp_.reset(new AudioTimestampHelper(
        bytes_per_frame, decoder_->samples_per_second()));
  }

  void InitializeWithStatus(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("InitializeWithStatus(%d)", expected));

    WaitableMessageLoopEvent event;
    renderer_->Initialize(
        demuxer_stream_,
        event.GetPipelineStatusCB(),
        base::Bind(&AudioRendererImplTest::OnStatistics,
                   base::Unretained(this)),
        base::Bind(&AudioRendererImplTest::OnUnderflow,
                   base::Unretained(this)),
        base::Bind(&AudioRendererImplTest::OnAudioTimeCallback,
                   base::Unretained(this)),
        ended_event_.GetClosure(),
        base::Bind(&AudioRendererImplTest::OnDisabled,
                   base::Unretained(this)),
        base::Bind(&AudioRendererImplTest::OnError,
                   base::Unretained(this)));
    event.RunAndWaitForStatus(expected);

    // We should have no reads.
    EXPECT_TRUE(read_cb_.is_null());
  }

  void Preroll() {
    Preroll(0, PIPELINE_OK);
  }

  void Preroll(int timestamp_ms, PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("Preroll(%d, %d)", timestamp_ms, expected));

    TimeDelta timestamp = TimeDelta::FromMilliseconds(timestamp_ms);
    next_timestamp_->SetBaseTimestamp(timestamp);

    // Fill entire buffer to complete prerolling.
    WaitableMessageLoopEvent event;
    renderer_->Preroll(timestamp, event.GetPipelineStatusCB());
    WaitForPendingRead();
    DeliverRemainingAudio();
    event.RunAndWaitForStatus(PIPELINE_OK);

    // We should have no reads.
    EXPECT_TRUE(read_cb_.is_null());
  }

  void Play() {
    SCOPED_TRACE("Play()");
    WaitableMessageLoopEvent event;
    renderer_->Play(event.GetClosure());
    renderer_->SetPlaybackRate(1.0f);
    event.RunAndWait();
  }

  void WaitForEnded() {
    SCOPED_TRACE("WaitForEnded()");
    ended_event_.RunAndWait();
  }

  void WaitForPendingRead() {
    SCOPED_TRACE("WaitForPendingRead()");
    if (!read_cb_.is_null())
      return;

    DCHECK(wait_for_pending_read_cb_.is_null());

    WaitableMessageLoopEvent event;
    wait_for_pending_read_cb_ = event.GetClosure();
    event.RunAndWait();

    DCHECK(!read_cb_.is_null());
    DCHECK(wait_for_pending_read_cb_.is_null());
  }

  // Delivers |size| bytes with value kPlayingAudio to |renderer_|.
  void SatisfyPendingRead(size_t size) {
    CHECK(!read_cb_.is_null());

    scoped_refptr<DataBuffer> buffer = new DataBuffer(size);
    buffer->SetDataSize(size);
    memset(buffer->GetWritableData(), kPlayingAudio, buffer->GetDataSize());
    buffer->SetTimestamp(next_timestamp_->GetTimestamp());
    buffer->SetDuration(next_timestamp_->GetDuration(buffer->GetDataSize()));
    next_timestamp_->AddBytes(buffer->GetDataSize());

    DeliverBuffer(AudioDecoder::kOk, buffer);
  }

  void AbortPendingRead() {
    DeliverBuffer(AudioDecoder::kAborted, NULL);
  }

  void DeliverEndOfStream() {
    DeliverBuffer(AudioDecoder::kOk, DataBuffer::CreateEOSBuffer());
  }

  // Delivers bytes until |renderer_|'s internal buffer is full and no longer
  // has pending reads.
  void DeliverRemainingAudio() {
    SatisfyPendingRead(bytes_remaining_in_buffer());
  }

  // Attempts to consume |size| bytes from |renderer_|'s internal buffer,
  // returning true if all |size| bytes were consumed, false if less than
  // |size| bytes were consumed.
  //
  // |muted| is optional and if passed will get set if the byte value of
  // the consumed data is muted audio.
  bool ConsumeBufferedData(uint32 size, bool* muted) {
    scoped_array<uint8> buffer(new uint8[size]);
    uint32 bytes_per_frame = (decoder_->bits_per_channel() / 8) *
        ChannelLayoutToChannelCount(decoder_->channel_layout());
    uint32 requested_frames = size / bytes_per_frame;
    uint32 frames_read = renderer_->FillBuffer(
        buffer.get(), requested_frames, 0);

    if (frames_read > 0 && muted) {
      *muted = (buffer[0] == kMutedAudio);
    }
    return (frames_read == requested_frames);
  }

  // Attempts to consume all data available from the renderer.  Returns the
  // number of frames read.  Since time is frozen, the audio delay will increase
  // as frames come in.
  int ConsumeAllBufferedData() {
    renderer_->DisableUnderflowForTesting();

    int frames_read = 0;
    int total_frames_read = 0;

    const int kRequestFrames = 1024;
    const uint32 bytes_per_frame = (decoder_->bits_per_channel() / 8) *
        ChannelLayoutToChannelCount(decoder_->channel_layout());
    scoped_array<uint8> buffer(new uint8[kRequestFrames * bytes_per_frame]);

    do {
      TimeDelta audio_delay = TimeDelta::FromMicroseconds(
          total_frames_read * Time::kMicrosecondsPerSecond /
          static_cast<float>(decoder_->samples_per_second()));

      frames_read = renderer_->FillBuffer(
          buffer.get(), kRequestFrames, audio_delay.InMilliseconds());
      total_frames_read += frames_read;
    } while (frames_read > 0);

    return total_frames_read * bytes_per_frame;
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

  TimeDelta CalculatePlayTime(int bytes_filled) {
    return TimeDelta::FromMicroseconds(
        bytes_filled * Time::kMicrosecondsPerSecond /
        renderer_->audio_parameters_.GetBytesPerSecond());
  }

  void EndOfStreamTest(float playback_rate) {
    Initialize();
    Preroll();
    Play();
    renderer_->SetPlaybackRate(playback_rate);

    // Drain internal buffer, we should have a pending read.
    int total_bytes = bytes_buffered();
    int bytes_filled = ConsumeAllBufferedData();
    WaitForPendingRead();

    // Due to how the cross-fade algorithm works we won't get an exact match
    // between the ideal and expected number of bytes consumed.  In the faster
    // than normal playback case, more bytes are created than should exist and
    // vice versa in the slower than normal playback case.
    const float kEpsilon = 0.10 * (total_bytes / playback_rate);
    EXPECT_NEAR(bytes_filled, total_bytes / playback_rate, kEpsilon);

    // Figure out how long until the ended event should fire.
    TimeDelta audio_play_time = CalculatePlayTime(bytes_filled);

    // Fulfill the read with an end-of-stream packet.  We shouldn't report ended
    // nor have a read until we drain the internal buffer.
    DeliverEndOfStream();

    // Advance time half way without an ended expectation.
    AdvanceTime(audio_play_time / 2);
    ConsumeBufferedData(bytes_buffered(), NULL);

    // Advance time by other half and expect the ended event.
    AdvanceTime(audio_play_time / 2);
    ConsumeBufferedData(bytes_buffered(), NULL);
    WaitForEnded();
  }

  void AdvanceTime(TimeDelta time) {
    base::AutoLock auto_lock(lock_);
    time_ += time;
  }

  // Fixture members.
  MessageLoop message_loop_;
  scoped_ptr<AudioRendererImpl> renderer_;

 private:
  Time GetTime() {
    base::AutoLock auto_lock(lock_);
    return time_;
  }

  void ReadDecoder(const AudioDecoder::ReadCB& read_cb) {
    // TODO(scherkus): Make this a DCHECK after threading semantics are fixed.
    if (MessageLoop::current() != &message_loop_) {
      message_loop_.PostTask(FROM_HERE, base::Bind(
          &AudioRendererImplTest::ReadDecoder,
          base::Unretained(this), read_cb));
      return;
    }

    CHECK(read_cb_.is_null()) << "Overlapping reads are not permitted";
    read_cb_ = read_cb;

    // Wake up WaitForPendingRead() if needed.
    if (!wait_for_pending_read_cb_.is_null())
      base::ResetAndReturn(&wait_for_pending_read_cb_).Run();
  }

  void DeliverBuffer(AudioDecoder::Status status,
                     const scoped_refptr<DataBuffer>& buffer) {
    CHECK(!read_cb_.is_null());
    base::ResetAndReturn(&read_cb_).Run(status, buffer);
  }

  scoped_refptr<MockDemuxerStream> demuxer_stream_;
  MockAudioDecoder* decoder_;

  // Used for stubbing out time in the audio callback thread.
  base::Lock lock_;
  Time time_;

  // Used for satisfying reads.
  AudioDecoder::ReadCB read_cb_;
  scoped_ptr<AudioTimestampHelper> next_timestamp_;
  AudioDecoderConfig audio_config_;

  WaitableMessageLoopEvent ended_event_;

  // Run during ReadDecoder() to unblock WaitForPendingRead().
  base::Closure wait_for_pending_read_cb_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImplTest);
};

TEST_F(AudioRendererImplTest, Initialize_Failed) {
  ExpectUnsupportedAudioDecoderConfig();
  InitializeWithStatus(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(AudioRendererImplTest, Initialize_Successful) {
  Initialize();
}

TEST_F(AudioRendererImplTest, Initialize_DecoderInitFailure) {
  ExpectUnsupportedAudioDecoder();
  InitializeWithStatus(DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(AudioRendererImplTest, Preroll) {
  Initialize();
  Preroll();
}

TEST_F(AudioRendererImplTest, Play) {
  Initialize();
  Preroll();
  Play();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered(), NULL));
  WaitForPendingRead();
}

TEST_F(AudioRendererImplTest, EndOfStream) {
  EndOfStreamTest(1.0);
}

TEST_F(AudioRendererImplTest, EndOfStream_FasterPlaybackSpeed) {
  EndOfStreamTest(2.0);
}

TEST_F(AudioRendererImplTest, EndOfStream_SlowerPlaybackSpeed) {
  EndOfStreamTest(0.5);
}

TEST_F(AudioRendererImplTest, Underflow) {
  Initialize();
  Preroll();
  Play();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered(), NULL));
  WaitForPendingRead();

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
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_FALSE(muted);
}

TEST_F(AudioRendererImplTest, Underflow_EndOfStream) {
  Initialize();
  Preroll();
  Play();

  // Figure out how long until the ended event should fire.  Since
  // ConsumeBufferedData() doesn't provide audio delay information, the time
  // until the ended event fires is equivalent to the longest buffered section,
  // which is the initial bytes_buffered() read.
  TimeDelta time_until_ended = CalculatePlayTime(bytes_buffered());

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered(), NULL));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  const size_t kDataSize = 1024;
  EXPECT_CALL(*this, OnUnderflow());
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));

  // Deliver a little bit of data.
  SatisfyPendingRead(kDataSize);
  WaitForPendingRead();

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
  EXPECT_EQ(kDataSize, bytes_buffered());
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_FALSE(muted);

  // Attempt to read to make sure we're truly at the end of stream.
  AdvanceTime(time_until_ended);
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_FALSE(muted);
  WaitForEnded();
}

TEST_F(AudioRendererImplTest, Underflow_ResumeFromCallback) {
  Initialize();
  Preroll();
  Play();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered(), NULL));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  const size_t kDataSize = 1024;
  EXPECT_CALL(*this, OnUnderflow())
      .WillOnce(Invoke(this, &AudioRendererImplTest::CallResumeAfterUnderflow));
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));

  // Verify after resuming that we're still not getting data.
  bool muted = false;
  EXPECT_EQ(0u, bytes_buffered());
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_TRUE(muted);

  // Deliver data, we should get non-muted audio.
  DeliverRemainingAudio();
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_FALSE(muted);
}

TEST_F(AudioRendererImplTest, AbortPendingRead_Preroll) {
  Initialize();

  // Start prerolling and wait for a read.
  WaitableMessageLoopEvent event;
  renderer_->Preroll(TimeDelta(), event.GetPipelineStatusCB());
  WaitForPendingRead();

  // Simulate the decoder aborting the pending read.
  AbortPendingRead();
  event.RunAndWaitForStatus(PIPELINE_OK);

  // Preroll again to a different timestamp and verify it completed normally.
  Preroll(1000, PIPELINE_OK);
}

TEST_F(AudioRendererImplTest, AbortPendingRead_Pause) {
  Initialize();

  Preroll();
  Play();

  // Partially drain internal buffer so we get a pending read.
  EXPECT_TRUE(ConsumeBufferedData(bytes_buffered() / 2, NULL));
  WaitForPendingRead();

  // Start pausing.
  WaitableMessageLoopEvent event;
  renderer_->Pause(event.GetClosure());

  // Simulate the decoder aborting the pending read.
  AbortPendingRead();
  event.RunAndWait();

  // Preroll again to a different timestamp and verify it completed normally.
  Preroll(1000, PIPELINE_OK);
}

}  // namespace media
