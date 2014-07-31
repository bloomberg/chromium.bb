// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_buffer_converter.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/audio_splicer.h"
#include "media/base/fake_audio_renderer_sink.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/audio_renderer_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::TimeDelta;
using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace media {

namespace {

// Since AudioBufferConverter is used due to different input/output sample
// rates, define some helper types to differentiate between the two.
struct InputFrames {
  explicit InputFrames(int value) : value(value) {}
  int value;
};

struct OutputFrames {
  explicit OutputFrames(int value) : value(value) {}
  int value;
};

}  // namespace

// Constants to specify the type of audio data used.
static AudioCodec kCodec = kCodecVorbis;
static SampleFormat kSampleFormat = kSampleFormatPlanarF32;
static ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static int kChannelCount = 2;
static int kChannels = ChannelLayoutToChannelCount(kChannelLayout);

// Use a different output sample rate so the AudioBufferConverter is invoked.
static int kInputSamplesPerSecond = 5000;
static int kOutputSamplesPerSecond = 10000;

ACTION_P(EnterPendingDecoderInitStateAction, test) {
  test->EnterPendingDecoderInitState(arg1);
}

class AudioRendererImplTest : public ::testing::Test {
 public:
  // Give the decoder some non-garbage media properties.
  AudioRendererImplTest()
      : hardware_config_(AudioParameters(), AudioParameters()),
        demuxer_stream_(DemuxerStream::AUDIO),
        decoder_(new MockAudioDecoder()),
        last_time_update_(kNoTimestamp()),
        ended_(false) {
    AudioDecoderConfig audio_config(kCodec,
                                    kSampleFormat,
                                    kChannelLayout,
                                    kInputSamplesPerSecond,
                                    NULL,
                                    0,
                                    false);
    demuxer_stream_.set_audio_decoder_config(audio_config);

    // Used to save callbacks and run them at a later time.
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillRepeatedly(Invoke(this, &AudioRendererImplTest::DecodeDecoder));
    EXPECT_CALL(*decoder_, Reset(_))
        .WillRepeatedly(Invoke(this, &AudioRendererImplTest::ResetDecoder));

    // Mock out demuxer reads.
    EXPECT_CALL(demuxer_stream_, Read(_)).WillRepeatedly(
        RunCallback<0>(DemuxerStream::kOk,
                       scoped_refptr<DecoderBuffer>(new DecoderBuffer(0))));
    EXPECT_CALL(demuxer_stream_, SupportsConfigChanges())
        .WillRepeatedly(Return(true));
    AudioParameters out_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               kChannelLayout,
                               kOutputSamplesPerSecond,
                               SampleFormatToBytesPerChannel(kSampleFormat) * 8,
                               512);
    hardware_config_.UpdateOutputConfig(out_params);
    ScopedVector<AudioDecoder> decoders;
    decoders.push_back(decoder_);
    sink_ = new FakeAudioRendererSink();
    renderer_.reset(new AudioRendererImpl(message_loop_.message_loop_proxy(),
                                          sink_,
                                          decoders.Pass(),
                                          SetDecryptorReadyCB(),
                                          &hardware_config_));
  }

  virtual ~AudioRendererImplTest() {
    SCOPED_TRACE("~AudioRendererImplTest()");
  }

  void ExpectUnsupportedAudioDecoder() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(DoAll(SaveArg<2>(&output_cb_),
                        RunCallback<1>(DECODER_ERROR_NOT_SUPPORTED)));
  }

  MOCK_METHOD1(OnStatistics, void(const PipelineStatistics&));
  MOCK_METHOD1(OnBufferingStateChange, void(BufferingState));
  MOCK_METHOD1(OnError, void(PipelineStatus));

  void OnAudioTimeCallback(TimeDelta current_time, TimeDelta max_time) {
    CHECK(current_time <= max_time);
    last_time_update_ = current_time;
  }

  void InitializeRenderer(const PipelineStatusCB& pipeline_status_cb) {
    renderer_->Initialize(
        &demuxer_stream_,
        pipeline_status_cb,
        base::Bind(&AudioRendererImplTest::OnStatistics,
                   base::Unretained(this)),
        base::Bind(&AudioRendererImplTest::OnAudioTimeCallback,
                   base::Unretained(this)),
        base::Bind(&AudioRendererImplTest::OnBufferingStateChange,
                   base::Unretained(this)),
        base::Bind(&AudioRendererImplTest::OnEnded,
                   base::Unretained(this)),
        base::Bind(&AudioRendererImplTest::OnError,
                   base::Unretained(this)));
  }

  void Initialize() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(DoAll(SaveArg<2>(&output_cb_),
                        RunCallback<1>(PIPELINE_OK)));
    InitializeWithStatus(PIPELINE_OK);

    next_timestamp_.reset(new AudioTimestampHelper(kInputSamplesPerSecond));
  }

  void InitializeWithStatus(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("InitializeWithStatus(%d)", expected));

    WaitableMessageLoopEvent event;
    InitializeRenderer(event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(expected);

    // We should have no reads.
    EXPECT_TRUE(decode_cb_.is_null());
  }

  void InitializeAndDestroy() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));

    WaitableMessageLoopEvent event;
    InitializeRenderer(event.GetPipelineStatusCB());

    // Destroy the |renderer_| before we let the MessageLoop run, this simulates
    // an interleaving in which we end up destroying the |renderer_| while the
    // OnDecoderSelected callback is in flight.
    renderer_.reset();
    event.RunAndWaitForStatus(PIPELINE_ERROR_ABORT);
  }

  void InitializeAndDestroyDuringDecoderInit() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(EnterPendingDecoderInitStateAction(this));

    WaitableMessageLoopEvent event;
    InitializeRenderer(event.GetPipelineStatusCB());
    base::RunLoop().RunUntilIdle();
    DCHECK(!init_decoder_cb_.is_null());

    renderer_.reset();
    event.RunAndWaitForStatus(PIPELINE_ERROR_ABORT);
  }

  void EnterPendingDecoderInitState(PipelineStatusCB cb) {
    init_decoder_cb_ = cb;
  }

  void FlushDuringPendingRead() {
    SCOPED_TRACE("FlushDuringPendingRead()");
    WaitableMessageLoopEvent flush_event;
    renderer_->Flush(flush_event.GetClosure());
    SatisfyPendingRead(InputFrames(256));
    flush_event.RunAndWait();

    EXPECT_FALSE(IsReadPending());
  }

  void Preroll() {
    Preroll(0, PIPELINE_OK);
  }

  void Preroll(int timestamp_ms, PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("Preroll(%d, %d)", timestamp_ms, expected));

    TimeDelta timestamp = TimeDelta::FromMilliseconds(timestamp_ms);
    next_timestamp_->SetBaseTimestamp(timestamp);

    // Fill entire buffer to complete prerolling.
    renderer_->SetMediaTime(timestamp);
    renderer_->StartPlaying();
    WaitForPendingRead();
    EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
    DeliverRemainingAudio();
  }

  void StartTicking() {
    renderer_->StartTicking();
    renderer_->SetPlaybackRate(1.0f);
  }

  void StopTicking() { renderer_->StopTicking(); }

  bool IsReadPending() const {
    return !decode_cb_.is_null();
  }

  void WaitForPendingRead() {
    SCOPED_TRACE("WaitForPendingRead()");
    if (!decode_cb_.is_null())
      return;

    DCHECK(wait_for_pending_decode_cb_.is_null());

    WaitableMessageLoopEvent event;
    wait_for_pending_decode_cb_ = event.GetClosure();
    event.RunAndWait();

    DCHECK(!decode_cb_.is_null());
    DCHECK(wait_for_pending_decode_cb_.is_null());
  }

  // Delivers decoded frames to |renderer_|.
  void SatisfyPendingRead(InputFrames frames) {
    CHECK_GT(frames.value, 0);
    CHECK(!decode_cb_.is_null());

    scoped_refptr<AudioBuffer> buffer =
        MakeAudioBuffer<float>(kSampleFormat,
                               kChannelLayout,
                               kChannelCount,
                               kInputSamplesPerSecond,
                               1.0f,
                               0.0f,
                               frames.value,
                               next_timestamp_->GetTimestamp());
    next_timestamp_->AddFrames(frames.value);

    DeliverBuffer(AudioDecoder::kOk, buffer);
  }

  void DeliverEndOfStream() {
    DCHECK(!decode_cb_.is_null());

    // Return EOS buffer to trigger EOS frame.
    EXPECT_CALL(demuxer_stream_, Read(_))
        .WillOnce(RunCallback<0>(DemuxerStream::kOk,
                                 DecoderBuffer::CreateEOSBuffer()));

    // Satify pending |decode_cb_| to trigger a new DemuxerStream::Read().
    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(base::ResetAndReturn(&decode_cb_), AudioDecoder::kOk));

    WaitForPendingRead();

    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(base::ResetAndReturn(&decode_cb_), AudioDecoder::kOk));

    base::RunLoop().RunUntilIdle();
  }

  // Delivers frames until |renderer_|'s internal buffer is full and no longer
  // has pending reads.
  void DeliverRemainingAudio() {
    while (frames_remaining_in_buffer().value > 0) {
      SatisfyPendingRead(InputFrames(256));
    }
  }

  // Attempts to consume |requested_frames| frames from |renderer_|'s internal
  // buffer. Returns true if and only if all of |requested_frames| were able
  // to be consumed.
  bool ConsumeBufferedData(OutputFrames requested_frames) {
    scoped_ptr<AudioBus> bus =
        AudioBus::Create(kChannels, requested_frames.value);
    int frames_read = 0;
    EXPECT_TRUE(sink_->Render(bus.get(), 0, &frames_read));
    return frames_read == requested_frames.value;
  }

  OutputFrames frames_buffered() {
    return OutputFrames(renderer_->algorithm_->frames_buffered());
  }

  OutputFrames buffer_capacity() {
    return OutputFrames(renderer_->algorithm_->QueueCapacity());
  }

  OutputFrames frames_remaining_in_buffer() {
    // This can happen if too much data was delivered, in which case the buffer
    // will accept the data but not increase capacity.
    if (frames_buffered().value > buffer_capacity().value) {
      return OutputFrames(0);
    }
    return OutputFrames(buffer_capacity().value - frames_buffered().value);
  }

  void force_config_change() {
    renderer_->OnConfigChange();
  }

  InputFrames converter_input_frames_left() const {
    return InputFrames(
        renderer_->buffer_converter_->input_frames_left_for_testing());
  }

  bool splicer_has_next_buffer() const {
    return renderer_->splicer_->HasNextBuffer();
  }

  base::TimeDelta last_time_update() const {
    return last_time_update_;
  }

  bool ended() const { return ended_; }

  // Fixture members.
  base::MessageLoop message_loop_;
  scoped_ptr<AudioRendererImpl> renderer_;
  scoped_refptr<FakeAudioRendererSink> sink_;
  AudioHardwareConfig hardware_config_;

 private:
  void DecodeDecoder(const scoped_refptr<DecoderBuffer>& buffer,
                     const AudioDecoder::DecodeCB& decode_cb) {
    // TODO(scherkus): Make this a DCHECK after threading semantics are fixed.
    if (base::MessageLoop::current() != &message_loop_) {
      message_loop_.PostTask(FROM_HERE, base::Bind(
          &AudioRendererImplTest::DecodeDecoder,
          base::Unretained(this), buffer, decode_cb));
      return;
    }

    CHECK(decode_cb_.is_null()) << "Overlapping decodes are not permitted";
    decode_cb_ = decode_cb;

    // Wake up WaitForPendingRead() if needed.
    if (!wait_for_pending_decode_cb_.is_null())
      base::ResetAndReturn(&wait_for_pending_decode_cb_).Run();
  }

  void ResetDecoder(const base::Closure& reset_cb) {
    if (!decode_cb_.is_null()) {
      // |reset_cb| will be called in DeliverBuffer(), after the decoder is
      // flushed.
      reset_cb_ = reset_cb;
      return;
    }

    message_loop_.PostTask(FROM_HERE, reset_cb);
  }

  void DeliverBuffer(AudioDecoder::Status status,
                     const scoped_refptr<AudioBuffer>& buffer) {
    CHECK(!decode_cb_.is_null());
    if (buffer && !buffer->end_of_stream())
      output_cb_.Run(buffer);
    base::ResetAndReturn(&decode_cb_).Run(status);

    if (!reset_cb_.is_null())
      base::ResetAndReturn(&reset_cb_).Run();

    base::RunLoop().RunUntilIdle();
  }

  void OnEnded() {
    CHECK(!ended_);
    ended_ = true;
  }

  MockDemuxerStream demuxer_stream_;
  MockAudioDecoder* decoder_;

  // Used for satisfying reads.
  AudioDecoder::OutputCB output_cb_;
  AudioDecoder::DecodeCB decode_cb_;
  base::Closure reset_cb_;
  scoped_ptr<AudioTimestampHelper> next_timestamp_;

  // Run during DecodeDecoder() to unblock WaitForPendingRead().
  base::Closure wait_for_pending_decode_cb_;

  PipelineStatusCB init_decoder_cb_;
  base::TimeDelta last_time_update_;
  bool ended_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImplTest);
};

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

TEST_F(AudioRendererImplTest, StartTicking) {
  Initialize();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();
}

TEST_F(AudioRendererImplTest, EndOfStream) {
  Initialize();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();

  // Forcefully trigger underflow.
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));

  // Fulfill the read with an end-of-stream buffer. Doing so should change our
  // buffering state so playback resumes.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  DeliverEndOfStream();

  // Consume all remaining data. We shouldn't have signal ended yet.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ended());

  // Ended should trigger on next render call.
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ended());
}

TEST_F(AudioRendererImplTest, Underflow) {
  Initialize();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers a buffering state change
  // update.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));

  // Verify we're still not getting audio data.
  EXPECT_EQ(0, frames_buffered().value);
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));

  // Deliver enough data to have enough for buffering.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  DeliverRemainingAudio();

  // Verify we're getting audio data.
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(1)));
}

TEST_F(AudioRendererImplTest, Underflow_CapacityResetsAfterFlush) {
  Initialize();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  OutputFrames initial_capacity = buffer_capacity();
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));

  // Verify that the buffer capacity increased as a result of underflowing.
  EXPECT_GT(buffer_capacity().value, initial_capacity.value);

  // Verify that the buffer capacity is restored to the |initial_capacity|.
  FlushDuringPendingRead();
  EXPECT_EQ(buffer_capacity().value, initial_capacity.value);
}

TEST_F(AudioRendererImplTest, Underflow_Flush) {
  Initialize();
  Preroll();
  StartTicking();

  // Force underflow.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));
  WaitForPendingRead();
  StopTicking();

  // We shouldn't expect another buffering state change when flushing.
  FlushDuringPendingRead();
}

TEST_F(AudioRendererImplTest, PendingRead_Flush) {
  Initialize();

  Preroll();
  StartTicking();

  // Partially drain internal buffer so we get a pending read.
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(256)));
  WaitForPendingRead();

  StopTicking();

  EXPECT_TRUE(IsReadPending());

  // Flush and expect to be notified that we have nothing.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  FlushDuringPendingRead();

  // Preroll again to a different timestamp and verify it completed normally.
  Preroll(1000, PIPELINE_OK);
}

TEST_F(AudioRendererImplTest, PendingRead_Destroy) {
  Initialize();

  Preroll();
  StartTicking();

  // Partially drain internal buffer so we get a pending read.
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(256)));
  WaitForPendingRead();

  StopTicking();

  EXPECT_TRUE(IsReadPending());

  renderer_.reset();
}

TEST_F(AudioRendererImplTest, PendingFlush_Destroy) {
  Initialize();

  Preroll();
  StartTicking();

  // Partially drain internal buffer so we get a pending read.
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(256)));
  WaitForPendingRead();

  StopTicking();

  EXPECT_TRUE(IsReadPending());

  // Start flushing.
  WaitableMessageLoopEvent flush_event;
  renderer_->Flush(flush_event.GetClosure());

  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  SatisfyPendingRead(InputFrames(256));

  renderer_.reset();
}

TEST_F(AudioRendererImplTest, InitializeThenDestroy) {
  InitializeAndDestroy();
}

TEST_F(AudioRendererImplTest, InitializeThenDestroyDuringDecoderInit) {
  InitializeAndDestroyDuringDecoderInit();
}

TEST_F(AudioRendererImplTest, ConfigChangeDrainsConverter) {
  Initialize();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();

  // Deliver a little bit of data.  Use an odd data size to ensure there is data
  // left in the AudioBufferConverter.  Ensure no buffers are in the splicer.
  SatisfyPendingRead(InputFrames(2053));
  EXPECT_FALSE(splicer_has_next_buffer());
  EXPECT_GT(converter_input_frames_left().value, 0);

  // Force a config change and then ensure all buffered data has been put into
  // the splicer.
  force_config_change();
  EXPECT_TRUE(splicer_has_next_buffer());
  EXPECT_EQ(0, converter_input_frames_left().value);
}

TEST_F(AudioRendererImplTest, TimeUpdatesOnFirstBuffer) {
  Initialize();
  Preroll();
  StartTicking();

  AudioTimestampHelper timestamp_helper(kOutputSamplesPerSecond);
  EXPECT_EQ(kNoTimestamp(), last_time_update());

  // Preroll() should be buffered some data, consume half of it now.
  OutputFrames frames_to_consume(frames_buffered().value / 2);
  EXPECT_TRUE(ConsumeBufferedData(frames_to_consume));
  WaitForPendingRead();
  base::RunLoop().RunUntilIdle();

  // ConsumeBufferedData() uses an audio delay of zero, so ensure we received
  // a time update that's equal to |kFramesToConsume| from above.
  timestamp_helper.SetBaseTimestamp(base::TimeDelta());
  timestamp_helper.AddFrames(frames_to_consume.value);
  EXPECT_EQ(timestamp_helper.GetTimestamp(), last_time_update());

  // The next time update should match the remaining frames_buffered(), but only
  // after running the message loop.
  frames_to_consume = frames_buffered();
  EXPECT_TRUE(ConsumeBufferedData(frames_to_consume));
  EXPECT_EQ(timestamp_helper.GetTimestamp(), last_time_update());

  base::RunLoop().RunUntilIdle();
  timestamp_helper.AddFrames(frames_to_consume.value);
  EXPECT_EQ(timestamp_helper.GetTimestamp(), last_time_update());
}

TEST_F(AudioRendererImplTest, ImmediateEndOfStream) {
  Initialize();
  {
    SCOPED_TRACE("Preroll()");
    renderer_->StartPlaying();
    WaitForPendingRead();
    EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
    DeliverEndOfStream();
  }
  StartTicking();

  // Read a single frame. We shouldn't be able to satisfy it.
  EXPECT_FALSE(ended());
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ended());
}

TEST_F(AudioRendererImplTest, OnRenderErrorCausesDecodeError) {
  Initialize();
  Preroll();
  StartTicking();

  EXPECT_CALL(*this, OnError(PIPELINE_ERROR_DECODE));
  sink_->OnRenderError();
  base::RunLoop().RunUntilIdle();
}

// Test for AudioRendererImpl calling Pause()/Play() on the sink when the
// playback rate is set to zero and non-zero.
TEST_F(AudioRendererImplTest, SetPlaybackRate) {
  Initialize();
  Preroll();

  // Rendering hasn't started. Sink should always be paused.
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(0.0f);
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(1.0f);
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());

  // Rendering has started with non-zero rate. Rate changes will affect sink
  // state.
  renderer_->StartTicking();
  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());
  renderer_->SetPlaybackRate(0.0f);
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(1.0f);
  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());

  // Rendering has stopped. Sink should be paused.
  renderer_->StopTicking();
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());

  // Start rendering with zero playback rate. Sink should be paused until
  // non-zero rate is set.
  renderer_->SetPlaybackRate(0.0f);
  renderer_->StartTicking();
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(1.0f);
  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());
}

}  // namespace media
