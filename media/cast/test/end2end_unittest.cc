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

#include <functional>
#include <list>
#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/cast_transport_sender.h"
#include "media/cast/transport/cast_transport_sender_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {

static const int64 kStartMillisecond = GG_INT64_C(1245);
static const int kAudioChannels = 2;
static const double kSoundFrequency = 314.15926535897;  // Freq of sine wave.
static const float kSoundVolume = 0.5f;
static const int kVideoHdWidth = 1280;
static const int kVideoHdHeight = 720;
static const int kVideoQcifWidth = 176;
static const int kVideoQcifHeight = 144;
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
// In addition, audio packets are sent in 10mS intervals in audio_encoder.cc,
// although we send an audio frame every 33mS, which adds an extra delay.
// A TODO was added in the code to resolve this.
static const int kTimerErrorMs = 20;

// Start the video synthetic start value to medium range value, to avoid edge
// effects cause by encoding and quantization.
static const int kVideoStart = 100;

std::string ConvertFromBase16String(const std::string base_16) {
  std::string compressed;
  DCHECK_EQ(base_16.size() % 2, 0u) << "Must be a multiple of 2";
  compressed.reserve(base_16.size() / 2);

  std::vector<uint8> v;
  if (!base::HexStringToBytes(base_16, &v)) {
    NOTREACHED();
  }
  compressed.assign(reinterpret_cast<const char*>(&v[0]), v.size());
  return compressed;
}

// Dummy callback function that does nothing except to accept ownership of
// |audio_bus| for destruction.
void OwnThatAudioBus(scoped_ptr<AudioBus> audio_bus) {}

void UpdateCastTransportStatus(transport::CastTransportStatus status) {
  EXPECT_EQ(status, transport::TRANSPORT_INITIALIZED);
}

// This is wrapped in a struct because it needs to be put into a std::map.
typedef struct {
  int counter[kNumOfLoggingEvents];
} LoggingEventCounts;

// Constructs a map from each frame (RTP timestamp) to counts of each event
// type logged for that frame.
std::map<RtpTimestamp, LoggingEventCounts> GetEventCountForFrameEvents(
    const std::vector<FrameEvent>& frame_events) {
  std::map<RtpTimestamp, LoggingEventCounts> event_counter_for_frame;
  for (std::vector<FrameEvent>::const_iterator it = frame_events.begin();
       it != frame_events.end();
       ++it) {
    std::map<RtpTimestamp, LoggingEventCounts>::iterator map_it =
        event_counter_for_frame.find(it->rtp_timestamp);
    if (map_it == event_counter_for_frame.end()) {
      LoggingEventCounts new_counter;
      memset(&new_counter, 0, sizeof(new_counter));
      ++(new_counter.counter[it->type]);
      event_counter_for_frame.insert(
          std::make_pair(it->rtp_timestamp, new_counter));
    } else {
      ++(map_it->second.counter[it->type]);
    }
  }
  return event_counter_for_frame;
}

// Constructs a map from each packet (Packet ID) to counts of each event
// type logged for that packet.
std::map<uint16, LoggingEventCounts> GetEventCountForPacketEvents(
    const std::vector<PacketEvent>& packet_events) {
  std::map<uint16, LoggingEventCounts> event_counter_for_packet;
  for (std::vector<PacketEvent>::const_iterator it = packet_events.begin();
       it != packet_events.end();
       ++it) {
    std::map<uint16, LoggingEventCounts>::iterator map_it =
        event_counter_for_packet.find(it->packet_id);
    if (map_it == event_counter_for_packet.end()) {
      LoggingEventCounts new_counter;
      memset(&new_counter, 0, sizeof(new_counter));
      ++(new_counter.counter[it->type]);
      event_counter_for_packet.insert(
          std::make_pair(it->packet_id, new_counter));
    } else {
      ++(map_it->second.counter[it->type]);
    }
  }
  return event_counter_for_packet;
}

}  // namespace

// Class that sends the packet direct from sender into the receiver with the
// ability to drop packets between the two.
class LoopBackTransport : public transport::PacketSender {
 public:
  explicit LoopBackTransport(scoped_refptr<CastEnvironment> cast_environment)
      : send_packets_(true),
        drop_packets_belonging_to_odd_frames_(false),
        reset_reference_frame_id_(false),
        cast_environment_(cast_environment) {}

  void SetPacketReceiver(
      const transport::PacketReceiverCallback& packet_receiver) {
    packet_receiver_ = packet_receiver;
  }

  virtual bool SendPacket(const Packet& packet) OVERRIDE {
    DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
    if (!send_packets_)
      return false;

    if (drop_packets_belonging_to_odd_frames_) {
      uint32 frame_id = packet[13];
      if (frame_id % 2 == 1)
        return true;
    }

    scoped_ptr<Packet> packet_copy(new Packet(packet));
    if (reset_reference_frame_id_) {
      // Reset the is_reference bit in the cast header.
      (*packet_copy)[kCommonRtpHeaderLength] &= kCastReferenceFrameIdBitReset;
    }
    packet_receiver_.Run(packet_copy.Pass());
    return true;
  }

  void SetSendPackets(bool send_packets) { send_packets_ = send_packets; }

  void DropAllPacketsBelongingToOddFrames() {
    drop_packets_belonging_to_odd_frames_ = true;
  }

  void AlwaysResetReferenceFrameId() { reset_reference_frame_id_ = true; }

 private:
  transport::PacketReceiverCallback packet_receiver_;
  bool send_packets_;
  bool drop_packets_belonging_to_odd_frames_;
  bool reset_reference_frame_id_;
  scoped_refptr<CastEnvironment> cast_environment_;
};

// Class that verifies the audio frames coming out of the receiver.
class TestReceiverAudioCallback
    : public base::RefCountedThreadSafe<TestReceiverAudioCallback> {
 public:
  struct ExpectedAudioFrame {
    PcmAudioFrame audio_frame;
    int num_10ms_blocks;
    base::TimeTicks record_time;
  };

  TestReceiverAudioCallback() : num_called_(0) {}

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

    const base::TimeTicks upper_bound =
        expected_audio_frame.record_time +
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
    if (audio_frame->samples.size() == 0)
      return;  // No more checks needed.

    EXPECT_NEAR(CountZeroCrossings(expected_audio_frame.audio_frame.samples),
                CountZeroCrossings(audio_frame->samples),
                1);
  }

  void CheckCodedPcmAudioFrame(
      scoped_ptr<transport::EncodedAudioFrame> audio_frame,
      const base::TimeTicks& playout_time) {
    ++num_called_;

    EXPECT_FALSE(expected_frame_.empty());  // Test for bug in test code.
    ExpectedAudioFrame expected_audio_frame = expected_frame_.front();
    expected_frame_.pop_front();

    EXPECT_EQ(static_cast<int>(audio_frame->data.size()),
              2 * kAudioChannels * expected_sampling_frequency_ / 100);

    base::TimeDelta time_since_recording =
        playout_time - expected_audio_frame.record_time;

    EXPECT_LE(time_since_recording,
              base::TimeDelta::FromMilliseconds(kDefaultRtpMaxDelayMs +
                                                kTimerErrorMs));

    EXPECT_LT(expected_audio_frame.record_time, playout_time);
    if (audio_frame->data.size() == 0)
      return;  // No more checks needed.

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

  int number_times_called() const { return num_called_; }

 protected:
  virtual ~TestReceiverAudioCallback() {}

 private:
  friend class base::RefCountedThreadSafe<TestReceiverAudioCallback>;

  int num_called_;
  int expected_sampling_frequency_;
  std::list<ExpectedAudioFrame> expected_frame_;
};

// Class that verifies the video frames coming out of the receiver.
class TestReceiverVideoCallback
    : public base::RefCountedThreadSafe<TestReceiverVideoCallback> {
 public:
  struct ExpectedVideoFrame {
    int start_value;
    int width;
    int height;
    base::TimeTicks capture_time;
  };

  TestReceiverVideoCallback() : num_called_(0) {}

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

  int number_times_called() const { return num_called_; }

 protected:
  virtual ~TestReceiverVideoCallback() {}

 private:
  friend class base::RefCountedThreadSafe<TestReceiverVideoCallback>;

  int num_called_;
  std::list<ExpectedVideoFrame> expected_frame_;
};

// The actual test class, generate synthetic data for both audio and video and
// send those through the sender and receiver and analyzes the result.
class End2EndTest : public ::testing::Test {
 protected:
  End2EndTest()
      : start_time_(),
        testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new test::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_,
            task_runner_,
            task_runner_,
            task_runner_,
            GetLoggingConfigWithRawEventsAndStatsEnabled())),
        receiver_to_sender_(cast_environment_),
        sender_to_receiver_(cast_environment_),
        test_receiver_audio_callback_(new TestReceiverAudioCallback()),
        test_receiver_video_callback_(new TestReceiverVideoCallback()) {
    testing_clock_->Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
  }

  void SetupConfig(transport::AudioCodec audio_codec,
                   int audio_sampling_frequency,
                   // TODO(miu): 3rd arg is meaningless?!?
                   bool external_audio_decoder,
                   int max_number_of_video_buffers_used) {
    audio_sender_config_.sender_ssrc = 1;
    audio_sender_config_.incoming_feedback_ssrc = 2;
    audio_sender_config_.rtp_config.payload_type = 96;
    audio_sender_config_.use_external_encoder = false;
    audio_sender_config_.frequency = audio_sampling_frequency;
    audio_sender_config_.channels = kAudioChannels;
    audio_sender_config_.bitrate = kDefaultAudioEncoderBitrate;
    audio_sender_config_.codec = audio_codec;

    audio_receiver_config_.feedback_ssrc =
        audio_sender_config_.incoming_feedback_ssrc;
    audio_receiver_config_.incoming_ssrc = audio_sender_config_.sender_ssrc;
    audio_receiver_config_.rtp_payload_type =
        audio_sender_config_.rtp_config.payload_type;
    audio_receiver_config_.use_external_decoder = external_audio_decoder;
    audio_receiver_config_.frequency = audio_sender_config_.frequency;
    audio_receiver_config_.channels = kAudioChannels;
    audio_receiver_config_.codec = audio_sender_config_.codec;

    test_receiver_audio_callback_->SetExpectedSamplingFrequency(
        audio_receiver_config_.frequency);

    video_sender_config_.sender_ssrc = 3;
    video_sender_config_.incoming_feedback_ssrc = 4;
    video_sender_config_.rtp_config.payload_type = 97;
    video_sender_config_.use_external_encoder = false;
    video_sender_config_.width = kVideoHdWidth;
    video_sender_config_.height = kVideoHdHeight;
    video_sender_config_.max_bitrate = 5000000;
    video_sender_config_.min_bitrate = 1000000;
    video_sender_config_.start_bitrate = 5000000;
    video_sender_config_.max_qp = 30;
    video_sender_config_.min_qp = 4;
    video_sender_config_.max_frame_rate = 30;
    video_sender_config_.max_number_of_video_buffers_used =
        max_number_of_video_buffers_used;
    video_sender_config_.codec = transport::kVp8;
    video_sender_config_.number_of_cores = 1;

    video_receiver_config_.feedback_ssrc =
        video_sender_config_.incoming_feedback_ssrc;
    video_receiver_config_.incoming_ssrc = video_sender_config_.sender_ssrc;
    video_receiver_config_.rtp_payload_type =
        video_sender_config_.rtp_config.payload_type;
    video_receiver_config_.use_external_decoder = false;
    video_receiver_config_.codec = video_sender_config_.codec;

    transport_config_.audio_ssrc = audio_sender_config_.sender_ssrc;
    transport_config_.video_ssrc = video_sender_config_.sender_ssrc;
    transport_config_.video_codec = video_sender_config_.codec;
    transport_config_.audio_codec = audio_sender_config_.codec;
    transport_config_.video_rtp_config = video_sender_config_.rtp_config;
    transport_config_.audio_rtp_config = audio_sender_config_.rtp_config;
    transport_config_.audio_frequency = audio_sender_config_.frequency;
    transport_config_.audio_channels = audio_sender_config_.channels;
  }

  void Create() {
    cast_receiver_.reset(
        CastReceiver::CreateCastReceiver(cast_environment_,
                                         audio_receiver_config_,
                                         video_receiver_config_,
                                         &receiver_to_sender_));
    transport_sender_.reset(new transport::CastTransportSenderImpl(
        testing_clock_,
        transport_config_,
        base::Bind(&UpdateCastTransportStatus),
        task_runner_,
        &sender_to_receiver_));

    cast_sender_.reset(CastSender::CreateCastSender(
        cast_environment_,
        &audio_sender_config_,
        &video_sender_config_,
        NULL,
        base::Bind(&End2EndTest::InitializationResult, base::Unretained(this)),
        transport_sender_.get()));

    receiver_to_sender_.SetPacketReceiver(cast_sender_->packet_receiver());
    sender_to_receiver_.SetPacketReceiver(cast_receiver_->packet_receiver());

    frame_input_ = cast_sender_->frame_input();
    frame_receiver_ = cast_receiver_->frame_receiver();

    audio_bus_factory_.reset(
        new TestAudioBusFactory(audio_sender_config_.channels,
                                audio_sender_config_.frequency,
                                kSoundFrequency,
                                kSoundVolume));
  }

  virtual ~End2EndTest() {
    cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
  }

  virtual void TearDown() OVERRIDE {

    cast_sender_.reset();
    cast_receiver_.reset();
    task_runner_->RunTasks();
  }

  void SendVideoFrame(int start_value, const base::TimeTicks& capture_time) {
    if (start_time_.is_null())
      start_time_ = capture_time;
    base::TimeDelta time_diff = capture_time - start_time_;
    gfx::Size size(video_sender_config_.width, video_sender_config_.height);
    EXPECT_TRUE(VideoFrame::IsValidConfig(
        VideoFrame::I420, size, gfx::Rect(size), size));
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
            VideoFrame::I420, size, gfx::Rect(size), size, time_diff);
    PopulateVideoFrame(video_frame, start_value);
    frame_input_->InsertRawVideoFrame(video_frame, capture_time);
  }

  void RunTasks(int during_ms) {
    for (int i = 0; i < during_ms; ++i) {
      // Call process the timers every 1 ms.
      testing_clock_->Advance(base::TimeDelta::FromMilliseconds(1));
      task_runner_->RunTasks();
    }
  }

  void InitializationResult(CastInitializationStatus result) {
    EXPECT_EQ(result, STATUS_INITIALIZED);
  }

  AudioReceiverConfig audio_receiver_config_;
  VideoReceiverConfig video_receiver_config_;
  AudioSenderConfig audio_sender_config_;
  VideoSenderConfig video_sender_config_;
  transport::CastTransportConfig transport_config_;

  base::TimeTicks start_time_;
  base::SimpleTestTickClock* testing_clock_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;

  LoopBackTransport receiver_to_sender_;
  LoopBackTransport sender_to_receiver_;
  scoped_ptr<transport::CastTransportSenderImpl> transport_sender_;

  scoped_ptr<CastReceiver> cast_receiver_;
  scoped_ptr<CastSender> cast_sender_;
  scoped_refptr<FrameInput> frame_input_;
  scoped_refptr<FrameReceiver> frame_receiver_;

  scoped_refptr<TestReceiverAudioCallback> test_receiver_audio_callback_;
  scoped_refptr<TestReceiverVideoCallback> test_receiver_video_callback_;

  scoped_ptr<TestAudioBusFactory> audio_bus_factory_;

  SimpleEventSubscriber event_subscriber_;
  std::vector<FrameEvent> frame_events_;
  std::vector<PacketEvent> packet_events_;
  std::vector<GenericEvent> generic_events_;
};

#if defined(OS_WIN)
#define MAYBE_LoopNoLossPcm16 DISABLED_LoopNoLossPcm16
#else
#define MAYBE_LoopNoLossPcm16 LoopNoLossPcm16
#endif
// TODO(mikhal): Crashes in win bots (http://crbug.com/329563)
TEST_F(End2EndTest, MAYBE_LoopNoLossPcm16) {
  SetupConfig(transport::kPcm16, 32000, false, 1);
  // Reduce video resolution to allow processing multiple frames within a
  // reasonable time frame.
  video_sender_config_.width = kVideoQcifWidth;
  video_sender_config_.height = kVideoQcifHeight;
  Create();

  int video_start = kVideoStart;
  int audio_diff = kFrameTimerMs;
  int i = 0;

  for (; i < 300; ++i) {
    int num_10ms_blocks = audio_diff / 10;
    audio_diff -= num_10ms_blocks * 10;

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    base::TimeTicks send_time = testing_clock_->NowTicks();
    if (i != 0) {
      // Due to the re-sampler and NetEq in the webrtc AudioCodingModule the
      // first samples will be 0 and then slowly ramp up to its real
      // amplitude;
      // ignore the first frame.
      test_receiver_audio_callback_->AddExpectedResult(
          ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
          num_10ms_blocks,
          send_time);
    }

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(
        audio_bus_ptr,
        send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    test_receiver_video_callback_->AddExpectedResult(
        video_start,
        video_sender_config_.width,
        video_sender_config_.height,
        send_time);
    SendVideoFrame(video_start, send_time);

    if (i == 0) {
      frame_receiver_->GetRawAudioFrame(
          num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::IgnoreAudioFrame,
                     test_receiver_audio_callback_));
    } else {
      frame_receiver_->GetRawAudioFrame(
          num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::CheckPcmAudioFrame,
                     test_receiver_audio_callback_));
    }

    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_));

    RunTasks(kFrameTimerMs);
    audio_diff += kFrameTimerMs;
    video_start++;
  }

  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(i - 1, test_receiver_audio_callback_->number_times_called());
  EXPECT_EQ(i, test_receiver_video_callback_->number_times_called());
}

// TODO(mikhal): Crashes on the Win7 x64 bots. Re-enable.
// http://crbug.com/329563
#if defined(OS_WIN)
#define MAYBE_LoopNoLossPcm16ExternalDecoder \
  DISABLED_LoopNoLossPcm16ExternalDecoder
#else
#define MAYBE_LoopNoLossPcm16ExternalDecoder LoopNoLossPcm16ExternalDecoder
#endif
// This tests our external decoder interface for Audio.
// Audio test without packet loss using raw PCM 16 audio "codec";
TEST_F(End2EndTest, MAYBE_LoopNoLossPcm16ExternalDecoder) {
  SetupConfig(transport::kPcm16, 32000, true, 1);
  Create();

  int i = 0;
  for (; i < 10; ++i) {
    base::TimeTicks send_time = testing_clock_->NowTicks();
    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10)));
    test_receiver_audio_callback_->AddExpectedResult(
        ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
        1,
        send_time);

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(
        audio_bus_ptr,
        send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    RunTasks(10);
    frame_receiver_->GetCodedAudioFrame(
        base::Bind(&TestReceiverAudioCallback::CheckCodedPcmAudioFrame,
                   test_receiver_audio_callback_));
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(10, test_receiver_audio_callback_->number_times_called());
}

// TODO(mikhal): Crashes on the bots. Re-enable. http://crbug.com/329563
#if defined(OS_WIN)
#define MAYBE_LoopNoLossOpus DISABLED_LoopNoLossOpus
#else
#define MAYBE_LoopNoLossOpus LoopNoLossOpus
#endif
// This tests our Opus audio codec without video.
TEST_F(End2EndTest, MAYBE_LoopNoLossOpus) {
  SetupConfig(transport::kOpus, kDefaultAudioSamplingRate, false, 1);
  Create();

  int i = 0;
  for (; i < 10; ++i) {
    int num_10ms_blocks = 3;
    base::TimeTicks send_time = testing_clock_->NowTicks();

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    if (i != 0) {
      test_receiver_audio_callback_->AddExpectedResult(
          ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
          num_10ms_blocks,
          send_time);
    }

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(
        audio_bus_ptr,
        send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    RunTasks(30);

    if (i == 0) {
      frame_receiver_->GetRawAudioFrame(
          num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::IgnoreAudioFrame,
                     test_receiver_audio_callback_));
    } else {
      frame_receiver_->GetRawAudioFrame(
          num_10ms_blocks,
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
  SetupConfig(transport::kOpus, kDefaultAudioSamplingRate, false, 1);
  Create();

  int video_start = kVideoStart;
  int audio_diff = kFrameTimerMs;

  sender_to_receiver_.SetSendPackets(false);

  for (int i = 0; i < 3; ++i) {
    int num_10ms_blocks = audio_diff / 10;
    audio_diff -= num_10ms_blocks * 10;

    base::TimeTicks send_time = testing_clock_->NowTicks();
    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(
        audio_bus_ptr,
        send_time,
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
    base::TimeTicks send_time = testing_clock_->NowTicks();

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    if (j >= number_of_audio_frames_to_ignore) {
      test_receiver_audio_callback_->AddExpectedResult(
          ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
          num_10ms_blocks,
          send_time);
    }

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(
        audio_bus_ptr,
        send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    test_receiver_video_callback_->AddExpectedResult(
        video_start,
        video_sender_config_.width,
        video_sender_config_.height,
        send_time);

    SendVideoFrame(video_start, send_time);
    RunTasks(kFrameTimerMs);
    audio_diff += kFrameTimerMs;

    if (j < number_of_audio_frames_to_ignore) {
      frame_receiver_->GetRawAudioFrame(
          num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::IgnoreAudioFrame,
                     test_receiver_audio_callback_));
    } else {
      frame_receiver_->GetRawAudioFrame(
          num_10ms_blocks,
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
  SetupConfig(transport::kOpus, kDefaultAudioSamplingRate, false, 3);
  video_sender_config_.rtp_config.max_delay_ms = 67;
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();

  int video_start = kVideoStart;
  base::TimeTicks send_time = testing_clock_->NowTicks();
  SendVideoFrame(video_start, send_time);
  RunTasks(kFrameTimerMs);

  test_receiver_video_callback_->AddExpectedResult(video_start,
                                                   video_sender_config_.width,
                                                   video_sender_config_.height,
                                                   send_time);
  frame_receiver_->GetRawVideoFrame(
      base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                 test_receiver_video_callback_));

  RunTasks(750);  // Make sure that we send a RTCP packet.

  video_start++;

  // Introduce a glitch lasting for 10 frames.
  sender_to_receiver_.SetSendPackets(false);
  for (int i = 0; i < 10; ++i) {
    send_time = testing_clock_->NowTicks();
    // First 3 will be sent and lost.
    SendVideoFrame(video_start, send_time);
    RunTasks(kFrameTimerMs);
    video_start++;
  }
  sender_to_receiver_.SetSendPackets(true);
  RunTasks(100);
  send_time = testing_clock_->NowTicks();

  // Frame 1 should be acked by now and we should have an opening to send 4.
  SendVideoFrame(video_start, send_time);
  RunTasks(kFrameTimerMs);

  // Frames 1-3 are old frames by now, and therefore should be decoded, but
  // not rendered. The next frame we expect to render is frame #4.
  test_receiver_video_callback_->AddExpectedResult(video_start,
                                                   video_sender_config_.width,
                                                   video_sender_config_.height,
                                                   send_time);

  frame_receiver_->GetRawVideoFrame(
      base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                 test_receiver_video_callback_));

  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(2, test_receiver_video_callback_->number_times_called());
}

TEST_F(End2EndTest, DropEveryOtherFrame3Buffers) {
  SetupConfig(transport::kOpus, kDefaultAudioSamplingRate, false, 3);
  video_sender_config_.rtp_config.max_delay_ms = 67;
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();
  sender_to_receiver_.DropAllPacketsBelongingToOddFrames();

  int video_start = kVideoStart;
  base::TimeTicks send_time;

  int i = 0;
  for (; i < 20; ++i) {
    send_time = testing_clock_->NowTicks();
    SendVideoFrame(video_start, send_time);

    if (i % 2 == 0) {
      test_receiver_video_callback_->AddExpectedResult(
          video_start,
          video_sender_config_.width,
          video_sender_config_.height,
          send_time);

      // GetRawVideoFrame will not return the frame until we are close in
      // time before we should render the frame.
      frame_receiver_->GetRawVideoFrame(
          base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                     test_receiver_video_callback_));
    }
    RunTasks(kFrameTimerMs);
    video_start++;
  }

  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(i / 2, test_receiver_video_callback_->number_times_called());
}

TEST_F(End2EndTest, ResetReferenceFrameId) {
  SetupConfig(transport::kOpus, kDefaultAudioSamplingRate, false, 3);
  video_sender_config_.rtp_config.max_delay_ms = 67;
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();
  sender_to_receiver_.AlwaysResetReferenceFrameId();

  int frames_counter = 0;
  for (; frames_counter < 20; ++frames_counter) {
    const base::TimeTicks send_time = testing_clock_->NowTicks();
    SendVideoFrame(frames_counter, send_time);

    test_receiver_video_callback_->AddExpectedResult(
        frames_counter,
        video_sender_config_.width,
        video_sender_config_.height,
        send_time);

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
  SetupConfig(transport::kPcm16, 32000, false, 1);

  transport_config_.aes_iv_mask =
      ConvertFromBase16String("1234567890abcdeffedcba0987654321");
  transport_config_.aes_key =
      ConvertFromBase16String("deadbeefcafeb0b0b0b0cafedeadbeef");

  video_receiver_config_.aes_iv_mask = transport_config_.aes_iv_mask;
  video_receiver_config_.aes_key = transport_config_.aes_key;

  Create();

  int frames_counter = 0;
  for (; frames_counter < 3; ++frames_counter) {
    const base::TimeTicks send_time = testing_clock_->NowTicks();

    SendVideoFrame(frames_counter, send_time);

    test_receiver_video_callback_->AddExpectedResult(
        frames_counter,
        video_sender_config_.width,
        video_sender_config_.height,
        send_time);

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

// TODO(mikhal): Crashes on the bots. Re-enable. http://crbug.com/329563
#if defined(OS_WIN)
#define MAYBE_CryptoAudio DISABLED_CryptoAudio
#else
#define MAYBE_CryptoAudio CryptoAudio
#endif
TEST_F(End2EndTest, MAYBE_CryptoAudio) {
  SetupConfig(transport::kPcm16, 32000, false, 1);

  transport_config_.aes_iv_mask =
      ConvertFromBase16String("abcdeffedcba12345678900987654321");
  transport_config_.aes_key =
      ConvertFromBase16String("deadbeefcafecafedeadbeefb0b0b0b0");

  audio_receiver_config_.aes_iv_mask = transport_config_.aes_iv_mask;
  audio_receiver_config_.aes_key = transport_config_.aes_key;

  Create();

  int frames_counter = 0;
  for (; frames_counter < 3; ++frames_counter) {
    int num_10ms_blocks = 2;

    const base::TimeTicks send_time = testing_clock_->NowTicks();

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    if (frames_counter != 0) {
      // Due to the re-sampler and NetEq in the webrtc AudioCodingModule the
      // first samples will be 0 and then slowly ramp up to its real
      // amplitude;
      // ignore the first frame.
      test_receiver_audio_callback_->AddExpectedResult(
          ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
          num_10ms_blocks,
          send_time);
    }
    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(
        audio_bus_ptr,
        send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    RunTasks(num_10ms_blocks * 10);

    if (frames_counter == 0) {
      frame_receiver_->GetRawAudioFrame(
          num_10ms_blocks,
          32000,
          base::Bind(&TestReceiverAudioCallback::IgnoreAudioFrame,
                     test_receiver_audio_callback_));
    } else {
      frame_receiver_->GetRawAudioFrame(
          num_10ms_blocks,
          32000,
          base::Bind(&TestReceiverAudioCallback::CheckPcmAudioFrame,
                     test_receiver_audio_callback_));
    }
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(frames_counter - 1,
            test_receiver_audio_callback_->number_times_called());
}

// Video test without packet loss - tests the logging aspects of the end2end,
// but is basically equivalent to LoopNoLossPcm16.
TEST_F(End2EndTest, VideoLogging) {
  SetupConfig(transport::kPcm16, 32000, false, 1);
  Create();

  int video_start = kVideoStart;
  const int num_frames = 1;
  for (int i = 0; i < num_frames; ++i) {
    base::TimeTicks send_time = testing_clock_->NowTicks();
    test_receiver_video_callback_->AddExpectedResult(
        video_start,
        video_sender_config_.width,
        video_sender_config_.height,
        send_time);

    SendVideoFrame(video_start, send_time);
    RunTasks(kFrameTimerMs);

    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_));

    video_start++;
  }

  // Basic tests.
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  int num_callbacks_called =
      test_receiver_video_callback_->number_times_called();
  EXPECT_EQ(num_frames, num_callbacks_called);

  RunTasks(750);  // Make sure that we send a RTCP message with the log.

  // Logging tests.
  // Frame logging.
  // Verify that all frames and all required events were logged.
  event_subscriber_.GetFrameEventsAndReset(&frame_events_);

  // For each frame, count the number of events that occurred for each event
  // for that frame.
  std::map<RtpTimestamp, LoggingEventCounts> event_counter_for_frame =
      GetEventCountForFrameEvents(frame_events_);

  // Verify that there are logs for expected number of frames.
  EXPECT_EQ(num_frames, static_cast<int>(event_counter_for_frame.size()));

  // Verify that each frame have the expected types of events logged.
  for (std::map<RtpTimestamp, LoggingEventCounts>::iterator map_it =
           event_counter_for_frame.begin();
       map_it != event_counter_for_frame.end();
       ++map_it) {
    int total_event_count_for_frame = 0;
    for (int i = 0; i < kNumOfLoggingEvents; ++i) {
      total_event_count_for_frame += map_it->second.counter[i];
    }

    int expected_event_count_for_frame = 0;

    EXPECT_GT(map_it->second.counter[kVideoFrameSentToEncoder], 0);
    expected_event_count_for_frame +=
        map_it->second.counter[kVideoFrameSentToEncoder];

    EXPECT_GT(map_it->second.counter[kVideoFrameEncoded], 0);
    expected_event_count_for_frame +=
        map_it->second.counter[kVideoFrameEncoded];

    EXPECT_GT(map_it->second.counter[kVideoFrameReceived], 0);
    expected_event_count_for_frame +=
        map_it->second.counter[kVideoFrameReceived];

    EXPECT_GT(map_it->second.counter[kVideoRenderDelay], 0);
    expected_event_count_for_frame += map_it->second.counter[kVideoRenderDelay];

    EXPECT_GT(map_it->second.counter[kVideoFrameDecoded], 0);
    expected_event_count_for_frame +=
        map_it->second.counter[kVideoFrameDecoded];

    // Verify that there were no other events logged with respect to this
    // frame.
    // (i.e. Total event count = expected event count)
    EXPECT_EQ(total_event_count_for_frame, expected_event_count_for_frame);
  }

  // Packet logging.
  // Verify that all packet related events were logged.
  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  std::map<uint16, LoggingEventCounts> event_count_for_packet =
      GetEventCountForPacketEvents(packet_events_);

  // Verify that each packet have the expected types of events logged.
  for (std::map<uint16, LoggingEventCounts>::iterator map_it =
           event_count_for_packet.begin();
       map_it != event_count_for_packet.end();
       ++map_it) {
    int total_event_count_for_packet = 0;
    for (int i = 0; i < kNumOfLoggingEvents; ++i) {
      total_event_count_for_packet += map_it->second.counter[i];
    }

    int expected_event_count_for_packet = 0;
    EXPECT_GT(map_it->second.counter[kVideoPacketReceived], 0);
    expected_event_count_for_packet +=
        map_it->second.counter[kVideoPacketReceived];

    // Verify that there were no other events logged with respect to this
    // packet. (i.e. Total event count = expected event count)
    EXPECT_EQ(total_event_count_for_packet, expected_event_count_for_packet);
  }
}

// TODO(mikhal): Crashes on the bots. Re-enable. http://crbug.com/329563
#if defined(OS_WIN)
#define MAYBE_AudioLogging DISABLED_AudioLogging
#else
#define MAYBE_AudioLogging AudioLogging
#endif
// Audio test without packet loss - tests the logging aspects of the end2end,
// but is basically equivalent to LoopNoLossPcm16.
TEST_F(End2EndTest, MAYBE_AudioLogging) {
  SetupConfig(transport::kPcm16, 32000, false, 1);
  Create();

  int audio_diff = kFrameTimerMs;
  const int num_frames = 10;
  for (int i = 0; i < num_frames; ++i) {
    int num_10ms_blocks = audio_diff / 10;
    audio_diff -= num_10ms_blocks * 10;
    base::TimeTicks send_time = testing_clock_->NowTicks();

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));

    if (i != 0) {
      // Due to the re-sampler and NetEq in the webrtc AudioCodingModule the
      // first samples will be 0 and then slowly ramp up to its real
      // amplitude;
      // ignore the first frame.
      test_receiver_audio_callback_->AddExpectedResult(
          ToPcmAudioFrame(*audio_bus, audio_sender_config_.frequency),
          num_10ms_blocks,
          send_time);
    }

    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(
        audio_bus_ptr,
        send_time,
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    RunTasks(kFrameTimerMs);
    audio_diff += kFrameTimerMs;

    if (i == 0) {
      frame_receiver_->GetRawAudioFrame(
          num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::IgnoreAudioFrame,
                     test_receiver_audio_callback_));
    } else {
      frame_receiver_->GetRawAudioFrame(
          num_10ms_blocks,
          audio_sender_config_.frequency,
          base::Bind(&TestReceiverAudioCallback::CheckPcmAudioFrame,
                     test_receiver_audio_callback_));
    }
  }

  // Basic tests.
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.

  int num_times_called = test_receiver_audio_callback_->number_times_called();
  EXPECT_EQ(num_frames - 1, num_times_called);

  // Logging tests.
  // Verify that all frames and all required events were logged.
  event_subscriber_.GetFrameEventsAndReset(&frame_events_);

  // Construct a map from each frame (RTP timestamp) to a count of each event
  // type logged for that frame.
  std::map<RtpTimestamp, LoggingEventCounts> event_counter_for_frame =
      GetEventCountForFrameEvents(frame_events_);

  // Verify that each frame have the expected types of events logged.
  std::map<RtpTimestamp, LoggingEventCounts>::iterator map_it =
      event_counter_for_frame.begin();

  // TODO(imcheng): This only checks the first frame. This doesn't work
  // properly for all frames for two reasons:
  // 1. There is a loopback of kAudioPlayoutDelay and kAudioFrameDecoded
  // events due to shared CastEnvironment between sender and
  // receiver in the test setup. We will need to create separate
  // CastEnvironment once again to fix this.
  // 2. kAudioPlayoutDelay and kAudioFrameDecoded RTP timestamps aren't
  // exactly aligned with those of kAudioFrameReceived and kAudioFrameEncoded.
  // Note that these RTP timestamps are output from webrtc::AudioCodingModule
  // which are different from RTP timestamps that the cast library generates
  // during the encode step (and which are sent to receiver). The first frame
  // just happen to be aligned.
  int total_event_count_for_frame = 0;
  for (int j = 0; j < kNumOfLoggingEvents; ++j)
    total_event_count_for_frame += map_it->second.counter[j];

  int expected_event_count_for_frame = 0;

  EXPECT_GT(map_it->second.counter[kAudioFrameReceived], 0);
  expected_event_count_for_frame += map_it->second.counter[kAudioFrameReceived];

  EXPECT_GT(map_it->second.counter[kAudioFrameEncoded], 0);
  expected_event_count_for_frame += map_it->second.counter[kAudioFrameEncoded];

  // Note that this is a big positive number instead of just 1 due to loopback
  // described in TODO above.
  EXPECT_GT(map_it->second.counter[kAudioPlayoutDelay], 0);
  expected_event_count_for_frame += map_it->second.counter[kAudioPlayoutDelay];
  EXPECT_GT(map_it->second.counter[kAudioFrameDecoded], 0);
  expected_event_count_for_frame += map_it->second.counter[kAudioFrameDecoded];

  // Verify that there were no other events logged with respect to this frame.
  // (i.e. Total event count = expected event count)
  EXPECT_EQ(total_event_count_for_frame, expected_event_count_for_frame);
}

// TODO(pwestin): Add repeatable packet loss test.
// TODO(pwestin): Add test for misaligned send get calls.
// TODO(pwestin): Add more tests that does not resample.
// TODO(pwestin): Add test when we have starvation for our RunTask.

}  // namespace cast
}  // namespace media
