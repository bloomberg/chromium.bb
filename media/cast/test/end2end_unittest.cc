// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test generate synthetic data. For audio it's a sinusoid waveform with
// frequency kSoundFrequency and different amplitudes. For video it's a pattern
// that is shifting by one pixel per frame, each pixels neighbors right and down
// is this pixels value +1, since the pixel value is 8 bit it will wrap
// frequently within the image. Visually this will create diagonally color bands
// that moves across the screen

#include <math.h>

#include <list>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/cast_sender.h"
#include "media/cast/test/audio_utility.h"
#include "media/cast/test/crypto_utility.h"
#include "media/cast/test/fake_task_runner.h"
#include "media/cast/test/video_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = GG_INT64_C(1245);
static const int kAudioChannels = 2;
static const double kSoundFrequency = 314.15926535897;  // Freq of sine wave.
static const float kSoundVolume = 0.5f;
static const int kVideoWidth = 1280;
static const int kVideoHeight = 720;
static const int kCommonRtpHeaderLength = 12;
static const uint8 kCastReferenceFrameIdBitReset = 0xDF;  // Mask is 0x40.

// Since the video encoded and decoded an error will be introduced; when
// comparing individual pixels the error can be quite large; we allow a PSNR of
// at least |kVideoAcceptedPSNR|.
static const double kVideoAcceptedPSNR = 38.0;

// The tests are commonly implemented with |kFrameTimerMs| RunTask function;
// a normal video is 30 fps hence the 33 ms between frames.
static const int kFrameTimerMs = 33;

// The packets pass through the pacer which can delay the beginning of the
// frame by 10 ms if there is packets belonging to the previous frame being
// retransmitted.
static const int kTimerErrorMs = 15;

namespace {
// Dummy callback function that does nothing except to accept ownership of
// |audio_bus| for destruction.
void OwnThatAudioBus(scoped_ptr<AudioBus> audio_bus) {
}
}  // namespace

// Class that sends the packet direct from sender into the receiver with the
// ability to drop packets between the two.
class LoopBackTransport : public PacketSender {
 public:
  explicit LoopBackTransport(scoped_refptr<CastEnvironment> cast_environment)
      : packet_receiver_(NULL),
        send_packets_(true),
        drop_packets_belonging_to_odd_frames_(false),
        reset_reference_frame_id_(false),
        cast_environment_(cast_environment) {
  }

  void RegisterPacketReceiver(PacketReceiver* packet_receiver) {
    DCHECK(packet_receiver);
    packet_receiver_ = packet_receiver;
  }

  virtual bool SendPacket(const Packet& packet) OVERRIDE {
    DCHECK(packet_receiver_);
    DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
    if (!send_packets_) return false;

    uint8* packet_copy = new uint8[packet.size()];
    memcpy(packet_copy, packet.data(), packet.size());
    packet_receiver_->ReceivedPacket(packet_copy, packet.size(),
        base::Bind(PacketReceiver::DeletePacket, packet_copy));
    return true;
  }

  virtual bool SendPackets(const PacketList& packets) OVERRIDE {
    DCHECK(packet_receiver_);
    DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
    if (!send_packets_) return false;

    for (size_t i = 0; i < packets.size(); ++i) {
      const Packet& packet = packets[i];
      if (drop_packets_belonging_to_odd_frames_) {
        uint32 frame_id = packet[13];
        if (frame_id % 2 == 1) continue;
      }
      uint8* packet_copy = new uint8[packet.size()];
      memcpy(packet_copy, packet.data(), packet.size());
      if (reset_reference_frame_id_) {
        // Reset the is_reference bit in the cast header.
        packet_copy[kCommonRtpHeaderLength] &= kCastReferenceFrameIdBitReset;
      }
      packet_receiver_->ReceivedPacket(packet_copy, packet.size(),
          base::Bind(PacketReceiver::DeletePacket, packet_copy));
    }
    return true;
  }

  void SetSendPackets(bool send_packets) {
    send_packets_ = send_packets;
  }

  void DropAllPacketsBelongingToOddFrames() {
    drop_packets_belonging_to_odd_frames_ = true;
  }

  void AlwaysResetReferenceFrameId() {
    reset_reference_frame_id_ = true;
  }

 private:
  PacketReceiver* packet_receiver_;
  bool send_packets_;
  bool drop_packets_belonging_to_odd_frames_;
  bool reset_reference_frame_id_;
  scoped_refptr<CastEnvironment> cast_environment_;
};

// Class that verifies the audio frames coming out of the receiver.
class TestReceiverAudioCallback :
     public base::RefCountedThreadSafe<TestReceiverAudioCallback>  {
 public:
  struct ExpectedAudioFrame {
    PcmAudioFrame audio_frame;
    int num_10ms_blocks;
    base::TimeTicks record_time;
  };

  TestReceiverAudioCallback()
      : num_called_(0) {}

  void SetExpectedSamplingFrequency(int expected_sampling_frequency) {
    expected_sampling_frequency_ = expected_sampling_frequency;
  }

  void AddExpectedResult(scoped_ptr<PcmAudioFrame> audio_frame,
                         int expected_num_10ms_blocks,
                         const base::TimeTicks& record_time) {
    ExpectedAudioFrame expected_audio_frame;
    expected_audio_frame.audio_frame = *audio_frame;
    expected_audio_frame.num_10ms_blocks = expected_num_10ms_blocks;
    expected_audio_frame.record_time = record_time;
    expected_frame_.push_back(expected_audio_frame);
  }

  void IgnoreAudioFrame(scoped_ptr<PcmAudioFrame> audio_frame,
                        const base::TimeTicks& playout_time) {}

  // Check the audio frame parameters but not the audio samples.
  void CheckBasicAudioFrame(const scoped_ptr<PcmAudioFrame>& audio_frame,
                            const base::TimeTicks& playout_time) {
    EXPECT_FALSE(expected_frame_.empty());  // Test for bug in test code.
    ExpectedAudioFrame expected_audio_frame = expected_frame_.front();
    EXPECT_EQ(audio_frame->channels, kAudioChannels);
    EXPECT_EQ(audio_frame->frequency, expected_sampling_frequency_);
    EXPECT_EQ(static_cast<int>(audio_frame->samples.size()),
        expected_audio_frame.num_10ms_blocks * kAudioChannels *
            expected_sampling_frequency_ / 100);

    const base::TimeTicks upper_bound = expected_audio_frame.record_time +
        base::TimeDelta::FromMilliseconds(kDefaultRtpMaxDelayMs +
                                          kTimerErrorMs);
    EXPECT_GE(upper_bound, playout_time)
        << "playout_time - upper_bound == "
        << (playout_time - upper_bound).InMicroseconds() << " usec";
    EXPECT_LT(expected_audio_frame.record_time, playout_time)
        << "playout_time - expected == "
        << (playout_time - expected_audio_frame.record_time).InMilliseconds()
        << " mS";

    EXPECT_EQ(audio_frame->samples.size(),
              expected_audio_frame.audio_frame.samples.size());
  }

  void CheckPcmAudioFrame(scoped_ptr<PcmAudioFrame> audio_frame,
                          const base::TimeTicks& playout_time) {
    ++num_called_;

    CheckBasicAudioFrame(audio_frame, playout_time);
    ExpectedAudioFrame expected_audio_frame = expected_frame_.front();
    expected_frame_.pop_front();
    if (audio_frame->samples.size() == 0) return;  // No more checks needed.

    EXPECT_NEAR(CountZeroCrossings(expected_audio_frame.audio_frame.samples),
                CountZeroCrossings(audio_frame->samples),
                1);
  }

  void CheckCodedPcmAudioFrame(scoped_ptr<EncodedAudioFrame> audio_frame,
                               const base::TimeTicks& playout_time) {
    ++num_called_;

    EXPECT_FALSE(expected_frame_.empty());  // Test for bug in test code.
    ExpectedAudioFrame expected_audio_frame = expected_frame_.front();
    expected_frame_.pop_front();

    EXPECT_EQ(static_cast<int>(audio_frame->data.size()),
        2 * kAudioChannels * expected_sampling_frequency_ / 100);

    base::TimeDelta time_since_recording =
       playout_time - expected_audio_frame.record_time;

    EXPECT_LE(time_since_recording, base::TimeDelta::FromMilliseconds(
        kDefaultRtpMaxDelayMs + kTimerErrorMs));

    EXPECT_LT(expected_audio_frame.record_time, playout_time);
    if (audio_frame->data.size() == 0) return;  // No more checks needed.

    // We need to convert our "coded" audio frame to our raw format.
    std::vector<int16> output_audio_samples;
    size_t number_of_samples = audio_frame->data.size() / 2;

    for (size_t i = 0; i < number_of_samples; ++i) {
      uint16 sample =
          static_cast<uint8>(audio_frame->data[1 + i * sizeof(uint16)]) +
          (static_cast<uint16>(audio_frame->data[i * sizeof(uint16)]) << 8);
      output_audio_samples.push_back(static_cast<int16>(sample));
    }

    EXPECT_NEAR(CountZeroCrossings(expected_audio_frame.audio_frame.samples),
                CountZeroCrossings(output_audio_samples),
                1);
  }

  int number_times_called() const {
    return num_called_;
  }

 protected:
  virtual ~TestReceiverAudioCallback() {}

 private:
  friend class base::RefCountedThreadSafe<TestReceiverAudioCallback>;

  int num_called_;
  int expected_sampling_frequency_;
  std::list<ExpectedAudioFrame> expected_frame_;
};

// Class that verifies the video frames coming out of the receiver.
class TestReceiverVideoCallback :
     public base::RefCountedThreadSafe<TestReceiverVideoCallback>  {
 public:
  struct ExpectedVideoFrame {
    int start_value;
    int width;
    int height;
    base::TimeTicks capture_time;
  };

  TestReceiverVideoCallback()
      : num_called_(0) {}

  void AddExpectedResult(int start_value,
                         int width,
                         int height,
                         const base::TimeTicks& capture_time) {
    ExpectedVideoFrame expected_video_frame;
    expected_video_frame.start_value = start_value;
    expected_video_frame.capture_time = capture_time;
    expected_video_frame.width = width;
    expected_video_frame.height = height;
    expected_frame_.push_back(expected_video_frame);
  }

  void CheckVideoFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                       const base::TimeTicks& render_time) {
    ++num_called_;

    EXPECT_FALSE(expected_frame_.empty());  // Test for bug in test code.
    ExpectedVideoFrame expected_video_frame = expected_frame_.front();
    expected_frame_.pop_front();

    base::TimeDelta time_since_capture =
       render_time - expected_video_frame.capture_time;
    const base::TimeDelta upper_bound = base::TimeDelta::FromMilliseconds(
        kDefaultRtpMaxDelayMs + kTimerErrorMs);

    EXPECT_GE(upper_bound, time_since_capture)
        << "time_since_capture - upper_bound == "
        << (time_since_capture - upper_bound).InMilliseconds() << " mS";
    EXPECT_LE(expected_video_frame.capture_time, render_time);
    EXPECT_EQ(expected_video_frame.width, video_frame->coded_size().width());
    EXPECT_EQ(expected_video_frame.height, video_frame->coded_size().height());

    gfx::Size size(expected_video_frame.width, expected_video_frame.height);
    scoped_refptr<media::VideoFrame> expected_I420_frame =
        media::VideoFrame::CreateFrame(
        VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(expected_I420_frame, expected_video_frame.start_value);

    EXPECT_GE(I420PSNR(expected_I420_frame, video_frame), kVideoAcceptedPSNR);
  }

  int number_times_called() { return num_called_;}

 protected:
  virtual ~TestReceiverVideoCallback() {}

 private:
  friend class base::RefCountedThreadSafe<TestReceiverVideoCallback>;

  int num_called_;
  std::list<ExpectedVideoFrame> expected_frame_;
};

CastLoggingConfig EnableCastLoggingConfig() {
  CastLoggingConfig config;
  config.enable_data_collection = true;
  return config;
}
// The actual test class, generate synthetic data for both audio and video and
// send those through the sender and receiver and analyzes the result.
class End2EndTest : public ::testing::Test {
 protected:
  End2EndTest()
      : task_runner_(new test::FakeTaskRunner(&testing_clock_)),
        cast_environment_(new CastEnvironment(&testing_clock_, task_runner_,
            task_runner_, task_runner_, task_runner_, task_runner_,
            EnableCastLoggingConfig())),
        start_time_(),
        sender_to_receiver_(cast_environment_),
        receiver_to_sender_(cast_environment_),
        test_receiver_audio_callback_(new TestReceiverAudioCallback()),
        test_receiver_video_callback_(new TestReceiverVideoCallback()) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  void SetupConfig(AudioCodec audio_codec,
                   int audio_sampling_frequency,
                   // TODO(miu): 3rd arg is meaningless?!?
                   bool external_audio_decoder,
                   int max_number_of_video_buffers_used) {
    audio_sender_config_.sender_ssrc = 1;
    audio_sender_config_.incoming_feedback_ssrc = 2;
    audio_sender_config_.rtp_payload_type = 96;
    audio_sender_config_.use_external_encoder = false;
    audio_sender_config_.frequency = audio_sampling_frequency;
    audio_sender_config_.channels = kAudioChannels;
    audio_sender_config_.bitrate = kDefaultAudioEncoderBitrate;
    audio_sender_config_.codec = audio_codec;

    audio_receiver_config_.feedback_ssrc =
        audio_sender_config_.incoming_feedback_ssrc;
    audio_receiver_config_.incoming_ssrc =
        audio_sender_config_.sender_ssrc;
    audio_receiver_config_.rtp_payload_type =
        audio_sender_config_.rtp_payload_type;
    audio_receiver_config_.use_external_decoder = external_audio_decoder;
    audio_receiver_config_.frequency = audio_sender_config_.frequency;
    audio_receiver_config_.channels = kAudioChannels;
    audio_receiver_config_.codec = audio_sender_config_.codec;

    test_receiver_audio_callback_->SetExpectedSamplingFrequency(
        audio_receiver_config_.frequency);

    video_sender_config_.sender_ssrc = 3;
    video_sender_config_.incoming_feedback_ssrc = 4;
    video_sender_config_.rtp_payload_type = 97;
    video_sender_config_.use_external_encoder = false;
    video_sender_config_.width = kVideoWidth;
    video_sender_config_.height = kVideoHeight;
    video_sender_config_.max_bitrate = 5000000;
    video_sender_config_.min_bitrate = 1000000;
    video_sender_config_.start_bitrate = 5000000;
    video_sender_config_.max_qp = 30;
    video_sender_config_.min_qp = 4;
    video_sender_config_.max_frame_rate = 30;
    video_sender_config_.max_number_of_video_buffers_used =
        max_number_of_video_buffers_used;
    video_sender_config_.codec = kVp8;
    video_sender_config_.number_of_cores = 1;

    video_receiver_config_.feedback_ssrc =
        video_sender_config_.incoming_feedback_ssrc;
    video_receiver_config_.incoming_ssrc =
        video_sender_config_.sender_ssrc;
    video_receiver_config_.rtp_payload_type =
        video_sender_config_.rtp_payload_type;
    video_receiver_config_.use_external_decoder = false;
    video_receiver_config_.codec = video_sender_config_.codec;
  }

  void Create() {
    cast_receiver_.reset(CastReceiver::CreateCastReceiver(cast_environment_,
        audio_receiver_config_, video_receiver_config_, &receiver_to_sender_));

    cast_sender_.reset(CastSender::CreateCastSender(cast_environment_,
                                                    audio_sender_config_,
                                                    video_sender_config_,
                                                    NULL,
                                                    &sender_to_receiver_));

    receiver_to_sender_.RegisterPacketReceiver(cast_sender_->packet_receiver());
    sender_to_receiver_.RegisterPacketReceiver(
        cast_receiver_->packet_receiver());

    frame_input_ = cast_sender_->frame_input();
    frame_receiver_ = cast_receiver_->frame_receiver();

    audio_bus_factory_.reset(new TestAudioBusFactory(
        audio_sender_config_.channels, audio_sender_config_.frequency,
        kSoundFrequency, kSoundVolume));
  }

  virtual ~End2EndTest() {}

  void SendVideoFrame(int start_value, const base::TimeTicks& capture_time) {
    if (start_time_.is_null())
      start_time_ = testing_clock_.NowTicks();
      start_time_ = testing_clock_.NowTicks();
    base::TimeDelta time_diff = testing_clock_.NowTicks() - start_time_;
    gfx::Size size(kVideoWidth, kVideoHeight);
    EXPECT_TRUE(VideoFrame::IsValidConfig(VideoFrame::I420,
                                          size, gfx::Rect(size), size));
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
        VideoFrame::I420, size, gfx::Rect(size), size, time_diff);
    PopulateVideoFrame(video_frame, start_value);
    frame_input_->InsertRawVideoFrame(video_frame, capture_time);
  }

  void RunTasks(int during_ms) {
    for (int i = 0; i < during_ms; ++i) {
      // Call process the timers every 1 ms.
      testing_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
      task_runner_->RunTasks();
    }
  }

  AudioReceiverConfig audio_receiver_config_;
  VideoReceiverConfig video_receiver_config_;
  AudioSenderConfig audio_sender_config_;
  VideoSenderConfig video_sender_config_;

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  base::TimeTicks start_time_;

  LoopBackTransport sender_to_receiver_;
  LoopBackTransport receiver_to_sender_;

  scoped_ptr<CastReceiver> cast_receiver_;
  scoped_ptr<CastSender> cast_sender_;
  scoped_refptr<FrameInput> frame_input_;
  scoped_refptr<FrameReceiver> frame_receiver_;

  scoped_refptr<TestReceiverAudioCallback> test_receiver_audio_callback_;
  scoped_refptr<TestReceiverVideoCallback> test_receiver_video_callback_;

  scoped_ptr<TestAudioBusFactory> audio_bus_factory_;
};

// Audio and video test without packet loss using raw PCM 16 audio "codec";
TEST_F(End2EndTest, LoopNoLossPcm16) {
  SetupConfig(kPcm16, 32000, false, 1);
  Create();

  int video_start = 1;
  int audio_diff = kFrameTimerMs;
  int i = 0;

  std::cout << "Progress ";
  for (; i < 300; ++i) {
    int num_10ms_blocks = audio_diff / 10;
    audio_diff -= num_10ms_blocks * 10;
    base::TimeTicks send_time = testing_clock_.NowTicks();

    test_receiver_video_callback_->AddExpectedResult(video_start,
        video_sender_config_.width, video_sender_config_.height, send_time);

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    if (i != 0) {
      // Due to the re-sampler and NetEq in the webrtc AudioCodingModule the
      // first samples will be 0 and then slowly ramp up to its real amplitude;
      // ignore the first frame.
      test_receiver_audio_callback_->AddExpectedResult(
          ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
          num_10ms_blocks, send_time);
    }

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(audio_bus_ptr, send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    SendVideoFrame(video_start, send_time);

    RunTasks(kFrameTimerMs);
    audio_diff += kFrameTimerMs;

    if (i == 0) {
      frame_receiver_->GetRawAudioFrame(num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::IgnoreAudioFrame,
              test_receiver_audio_callback_));
    } else {
      frame_receiver_->GetRawAudioFrame(num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::CheckPcmAudioFrame,
              test_receiver_audio_callback_));
    }

    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
            test_receiver_video_callback_));

    std::cout << " " << i << std::flush;
    video_start++;
  }
  std::cout << std::endl;

  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(i - 1, test_receiver_audio_callback_->number_times_called());
  EXPECT_EQ(i, test_receiver_video_callback_->number_times_called());
}

// This tests our external decoder interface for Audio.
// Audio test without packet loss using raw PCM 16 audio "codec";
TEST_F(End2EndTest, LoopNoLossPcm16ExternalDecoder) {
  SetupConfig(kPcm16, 32000, true, 1);
  Create();

  int i = 0;
  for (; i < 100; ++i) {
    base::TimeTicks send_time = testing_clock_.NowTicks();
    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10)));
    test_receiver_audio_callback_->AddExpectedResult(
        ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
        1, send_time);

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(audio_bus_ptr, send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    RunTasks(10);
    frame_receiver_->GetCodedAudioFrame(
        base::Bind(&TestReceiverAudioCallback::CheckCodedPcmAudioFrame,
            test_receiver_audio_callback_));
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(100, test_receiver_audio_callback_->number_times_called());
}

// This tests our Opus audio codec without video.
TEST_F(End2EndTest, LoopNoLossOpus) {
  SetupConfig(kOpus, kDefaultAudioSamplingRate, false, 1);
  Create();

  int i = 0;
  for (; i < 100; ++i) {
    int num_10ms_blocks = 3;
    base::TimeTicks send_time = testing_clock_.NowTicks();

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    if (i != 0) {
      test_receiver_audio_callback_->AddExpectedResult(
          ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
          num_10ms_blocks, send_time);
    }

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(audio_bus_ptr, send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    RunTasks(30);

    if (i == 0) {
      frame_receiver_->GetRawAudioFrame(num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::IgnoreAudioFrame,
              test_receiver_audio_callback_));
    } else {
      frame_receiver_->GetRawAudioFrame(num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::CheckPcmAudioFrame,
              test_receiver_audio_callback_));
    }
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(i - 1, test_receiver_audio_callback_->number_times_called());
}

// This tests start sending audio and video before the receiver is ready.
//
// TODO(miu): Test disabled because of non-determinism.
// http://crbug.com/314233
TEST_F(End2EndTest, DISABLED_StartSenderBeforeReceiver) {
  SetupConfig(kOpus, kDefaultAudioSamplingRate, false, 1);
  Create();

  int video_start = 1;
  int audio_diff = kFrameTimerMs;

  sender_to_receiver_.SetSendPackets(false);

  for (int i = 0; i < 3; ++i) {
    int num_10ms_blocks = audio_diff / 10;
    audio_diff -= num_10ms_blocks * 10;

    base::TimeTicks send_time = testing_clock_.NowTicks();
    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(audio_bus_ptr, send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    SendVideoFrame(video_start, send_time);
    RunTasks(kFrameTimerMs);
    audio_diff += kFrameTimerMs;
    video_start++;
  }
  RunTasks(100);
  sender_to_receiver_.SetSendPackets(true);

  int j = 0;
  const int number_of_audio_frames_to_ignore = 3;
  for (; j < 10; ++j) {
    int num_10ms_blocks = audio_diff / 10;
    audio_diff -= num_10ms_blocks * 10;
    base::TimeTicks send_time = testing_clock_.NowTicks();

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    if (j >= number_of_audio_frames_to_ignore) {
      test_receiver_audio_callback_->AddExpectedResult(
          ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
          num_10ms_blocks, send_time);
    }

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(audio_bus_ptr, send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    test_receiver_video_callback_->AddExpectedResult(video_start,
        video_sender_config_.width, video_sender_config_.height, send_time);

    SendVideoFrame(video_start, send_time);
    RunTasks(kFrameTimerMs);
    audio_diff += kFrameTimerMs;

    if (j < number_of_audio_frames_to_ignore) {
      frame_receiver_->GetRawAudioFrame(num_10ms_blocks,
         audio_sender_config_.frequency,
         base::Bind(&TestReceiverAudioCallback::IgnoreAudioFrame,
              test_receiver_audio_callback_));
     } else {
      frame_receiver_->GetRawAudioFrame(num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::CheckPcmAudioFrame,
              test_receiver_audio_callback_));
    }
    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
            test_receiver_video_callback_));
    video_start++;
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(j - number_of_audio_frames_to_ignore,
            test_receiver_audio_callback_->number_times_called());
  EXPECT_EQ(j, test_receiver_video_callback_->number_times_called());
}

// This tests a network glitch lasting for 10 video frames.
TEST_F(End2EndTest, GlitchWith3Buffers) {
  SetupConfig(kOpus, kDefaultAudioSamplingRate, false, 3);
  video_sender_config_.rtp_max_delay_ms = 67;
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();

  int video_start = 50;
  base::TimeTicks send_time = testing_clock_.NowTicks();
  SendVideoFrame(video_start, send_time);
  RunTasks(kFrameTimerMs);

  test_receiver_video_callback_->AddExpectedResult(video_start,
      video_sender_config_.width, video_sender_config_.height, send_time);
  frame_receiver_->GetRawVideoFrame(
      base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
          test_receiver_video_callback_));

  RunTasks(750);  // Make sure that we send a RTCP packet.

  video_start++;

  // Introduce a glitch lasting for 10 frames.
  sender_to_receiver_.SetSendPackets(false);
  for (int i = 0; i < 10; ++i) {
    send_time = testing_clock_.NowTicks();
    // First 3 will be sent and lost.
    SendVideoFrame(video_start, send_time);
    RunTasks(kFrameTimerMs);
    video_start++;
  }
  sender_to_receiver_.SetSendPackets(true);
  RunTasks(100);
  send_time = testing_clock_.NowTicks();

  // Frame 1 should be acked by now and we should have an opening to send 4.
  SendVideoFrame(video_start, send_time);
  RunTasks(kFrameTimerMs);

  test_receiver_video_callback_->AddExpectedResult(video_start,
      video_sender_config_.width, video_sender_config_.height, send_time);

  frame_receiver_->GetRawVideoFrame(
      base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
          test_receiver_video_callback_));

  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(2, test_receiver_video_callback_->number_times_called());
}

TEST_F(End2EndTest, DropEveryOtherFrame3Buffers) {
  SetupConfig(kOpus, kDefaultAudioSamplingRate, false, 3);
  video_sender_config_.rtp_max_delay_ms = 67;
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();
  sender_to_receiver_.DropAllPacketsBelongingToOddFrames();

  int video_start = 50;
  base::TimeTicks send_time;

  std::cout << "Progress ";
  int i = 0;
  for (; i < 20; ++i) {
    send_time = testing_clock_.NowTicks();
    SendVideoFrame(video_start, send_time);

    if (i % 2 == 0) {
      test_receiver_video_callback_->AddExpectedResult(video_start,
          video_sender_config_.width, video_sender_config_.height, send_time);

      // GetRawVideoFrame will not return the frame until we are close in
      // time before we should render the frame.
      frame_receiver_->GetRawVideoFrame(
          base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
              test_receiver_video_callback_));
    }
    RunTasks(kFrameTimerMs);
    std::cout << " " << i << std::flush;
    video_start++;
  }
  std::cout << std::endl;
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(i / 2, test_receiver_video_callback_->number_times_called());
}

TEST_F(End2EndTest, ResetReferenceFrameId) {
  SetupConfig(kOpus, kDefaultAudioSamplingRate, false, 3);
  video_sender_config_.rtp_max_delay_ms = 67;
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();
  sender_to_receiver_.AlwaysResetReferenceFrameId();

  int frames_counter = 0;
  for (; frames_counter < 20; ++frames_counter) {
    const base::TimeTicks send_time = testing_clock_.NowTicks();
    SendVideoFrame(frames_counter, send_time);

    test_receiver_video_callback_->AddExpectedResult(frames_counter,
        video_sender_config_.width, video_sender_config_.height, send_time);

    // GetRawVideoFrame will not return the frame until we are close to the
    // time in which we should render the frame.
    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_));
    RunTasks(kFrameTimerMs);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(frames_counter,
            test_receiver_video_callback_->number_times_called());
}

TEST_F(End2EndTest, CryptoVideo) {
  SetupConfig(kPcm16, 32000, false, 1);

  video_sender_config_.aes_iv_mask =
      ConvertFromBase16String("1234567890abcdeffedcba0987654321");
  video_sender_config_.aes_key =
      ConvertFromBase16String("deadbeefcafeb0b0b0b0cafedeadbeef");

  video_receiver_config_.aes_iv_mask = video_sender_config_.aes_iv_mask;
  video_receiver_config_.aes_key = video_sender_config_.aes_key;

  Create();

  int frames_counter = 0;
  for (; frames_counter < 20; ++frames_counter) {
    const base::TimeTicks send_time = testing_clock_.NowTicks();

    SendVideoFrame(frames_counter, send_time);

    test_receiver_video_callback_->AddExpectedResult(frames_counter,
        video_sender_config_.width, video_sender_config_.height, send_time);

    // GetRawVideoFrame will not return the frame until we are close to the
    // time in which we should render the frame.
    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_));
    RunTasks(kFrameTimerMs);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(frames_counter,
            test_receiver_video_callback_->number_times_called());
}

TEST_F(End2EndTest, CryptoAudio) {
  SetupConfig(kPcm16, 32000, false, 1);

  audio_sender_config_.aes_iv_mask =
     ConvertFromBase16String("abcdeffedcba12345678900987654321");
  audio_sender_config_.aes_key =
     ConvertFromBase16String("deadbeefcafecafedeadbeefb0b0b0b0");

  audio_receiver_config_.aes_iv_mask = audio_sender_config_.aes_iv_mask;
  audio_receiver_config_.aes_key = audio_sender_config_.aes_key;

  Create();

  int frames_counter = 0;
  for (; frames_counter < 20; ++frames_counter) {
    int num_10ms_blocks = 2;

    const base::TimeTicks send_time = testing_clock_.NowTicks();

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    if (frames_counter != 0) {
      // Due to the re-sampler and NetEq in the webrtc AudioCodingModule the
      // first samples will be 0 and then slowly ramp up to its real amplitude;
      // ignore the first frame.
      test_receiver_audio_callback_->AddExpectedResult(
          ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
          num_10ms_blocks, send_time);
    }
    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(audio_bus_ptr, send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    RunTasks(num_10ms_blocks * 10);

    if (frames_counter == 0) {
      frame_receiver_->GetRawAudioFrame(num_10ms_blocks,
          32000,
          base::Bind(&TestReceiverAudioCallback::IgnoreAudioFrame,
              test_receiver_audio_callback_));
    } else {
      frame_receiver_->GetRawAudioFrame(num_10ms_blocks,
          32000,
          base::Bind(&TestReceiverAudioCallback::CheckPcmAudioFrame,
              test_receiver_audio_callback_));
    }
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(frames_counter - 1,
            test_receiver_audio_callback_->number_times_called());
}

// Video test without packet loss; This test is targeted at testing the logging
// aspects of the end2end, but is basically equivalent to LoopNoLossPcm16.
TEST_F(End2EndTest, VideoLogging) {
  SetupConfig(kPcm16, 32000, false, 1);
  Create();

  int video_start = 1;
  int i = 0;
  for (; i < 1; ++i) {
    base::TimeTicks send_time = testing_clock_.NowTicks();
    test_receiver_video_callback_->AddExpectedResult(video_start,
        video_sender_config_.width, video_sender_config_.height, send_time);

    SendVideoFrame(video_start, send_time);
    RunTasks(kFrameTimerMs);

    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
            test_receiver_video_callback_));

    video_start++;
  }

  // Basic tests.
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(i, test_receiver_video_callback_->number_times_called());
  // Logging tests.
  LoggingImpl* logging = cast_environment_->Logging();

  // Frame logging.

  // Verify that all frames and all required events were logged.
  FrameRawMap frame_raw_log = logging->GetFrameRawData();
  // Every frame should have only one entry.
  EXPECT_EQ(static_cast<unsigned int>(i), frame_raw_log.size());
  FrameRawMap::const_iterator frame_it = frame_raw_log.begin();
  // Choose a video frame, and verify that all events were logged.
  std::vector<CastLoggingEvent> event_log = frame_it->second.type;
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kVideoFrameReceived)) != event_log.end());
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kVideoFrameSentToEncoder)) != event_log.end());
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kVideoFrameEncoded)) != event_log.end());
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kVideoRenderDelay)) != event_log.end());
  // TODO(mikhal): Plumb this one through.
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kVideoFrameDecoded)) == event_log.end());
  // Verify that there were no other events logged with respect to this frame.
  EXPECT_EQ(4u, event_log.size());

  // Packet logging.
  // Verify that all packet related events were logged.
  PacketRawMap packet_raw_log = logging->GetPacketRawData();
  // Every rtp_timestamp should have only one entry.
  EXPECT_EQ(static_cast<unsigned int>(i), packet_raw_log.size());
  PacketRawMap::const_iterator packet_it = packet_raw_log.begin();
  // Choose a packet, and verify that all events were logged.
  event_log = (++(packet_it->second.packet_map.begin()))->second.type;
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kPacketSentToPacer)) != event_log.end());
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kPacketSentToNetwork)) != event_log.end());
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kPacketReceived)) != event_log.end());
  // Verify that there were no other events logged with respect to this frame.
  EXPECT_EQ(3u, event_log.size());
}

// Audio test without packet loss; This test is targeted at testing the logging
// aspects of the end2end, but is basically equivalent to LoopNoLossPcm16.
TEST_F(End2EndTest, AudioLogging) {
  SetupConfig(kPcm16, 32000, false, 1);
  Create();

  int audio_diff = kFrameTimerMs;
  int i = 0;

  for (; i < 10; ++i) {
    int num_10ms_blocks = audio_diff / 10;
    audio_diff -= num_10ms_blocks * 10;
    base::TimeTicks send_time = testing_clock_.NowTicks();

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    if (i != 0) {
      // Due to the re-sampler and NetEq in the webrtc AudioCodingModule the
      // first samples will be 0 and then slowly ramp up to its real amplitude;
      // ignore the first frame.
      test_receiver_audio_callback_->AddExpectedResult(
          ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
          num_10ms_blocks, send_time);
    }

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(audio_bus_ptr, send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    RunTasks(kFrameTimerMs);
    audio_diff += kFrameTimerMs;

    if (i == 0) {
      frame_receiver_->GetRawAudioFrame(num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::IgnoreAudioFrame,
              test_receiver_audio_callback_));
    } else {
      frame_receiver_->GetRawAudioFrame(num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::CheckPcmAudioFrame,
              test_receiver_audio_callback_));
    }
  }

  // Basic tests.
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  //EXPECT_EQ(i - 1, test_receiver_audio_callback_->number_times_called());
  EXPECT_EQ(i - 1, test_receiver_audio_callback_->number_times_called());
  // Logging tests.
  LoggingImpl* logging = cast_environment_->Logging();
  // Verify that all frames and all required events were logged.
  FrameRawMap frame_raw_log = logging->GetFrameRawData();
  // TODO(mikhal): Results are wrong. Need to resolve passing/calculation of
  // rtp_timestamp for audio for this to work.
  // Should have logged both audio and video. Every frame should have only one
  // entry.
  //EXPECT_EQ(static_cast<unsigned int>(i - 1), frame_raw_log.size());
  FrameRawMap::const_iterator frame_it = frame_raw_log.begin();
  // Choose a video frame, and verify that all events were logged.
  std::vector<CastLoggingEvent> event_log = frame_it->second.type;
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kAudioFrameReceived)) != event_log.end());
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kAudioFrameEncoded)) != event_log.end());
  // EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
  //              kAudioPlayoutDelay)) != event_log.end());
  // TODO(mikhal): Plumb this one through.
  EXPECT_TRUE((std::find(event_log.begin(), event_log.end(),
               kAudioFrameDecoded)) == event_log.end());
  // Verify that there were no other events logged with respect to this frame.
  EXPECT_EQ(2u, event_log.size());
}


// TODO(pwestin): Add repeatable packet loss test.
// TODO(pwestin): Add test for misaligned send get calls.
// TODO(pwestin): Add more tests that does not resample.
// TODO(pwestin): Add test when we have starvation for our RunTask.

}  // namespace cast
}  // namespace media
