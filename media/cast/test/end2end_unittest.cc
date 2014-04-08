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
#include <stdint.h>

#include <functional>
#include <list>
#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/base/audio_bus.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/cast_transport_sender.h"
#include "media/cast/transport/cast_transport_sender_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {

static const int64 kStartMillisecond = INT64_C(1245);
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

// The size of audio frames.  The encoder joins/breaks all inserted audio into
// chunks of this size.
static const int kAudioFrameDurationMs = 10;

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

void UpdateCastTransportStatus(transport::CastTransportStatus status) {
  bool result = (status == transport::TRANSPORT_AUDIO_INITIALIZED ||
                 status == transport::TRANSPORT_VIDEO_INITIALIZED);
  EXPECT_TRUE(result);
}

void AudioInitializationStatus(CastInitializationStatus status) {
  EXPECT_EQ(STATUS_AUDIO_INITIALIZED, status);
}

void VideoInitializationStatus(CastInitializationStatus status) {
  EXPECT_EQ(STATUS_VIDEO_INITIALIZED, status);
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
    scoped_ptr<AudioBus> audio_bus;
    base::TimeTicks record_time;
  };

  TestReceiverAudioCallback() : num_called_(0) {}

  void SetExpectedSamplingFrequency(int expected_sampling_frequency) {
    expected_sampling_frequency_ = expected_sampling_frequency;
  }

  void AddExpectedResult(const AudioBus& audio_bus,
                         const base::TimeTicks& record_time) {
    scoped_ptr<ExpectedAudioFrame> expected_audio_frame(
        new ExpectedAudioFrame());
    expected_audio_frame->audio_bus =
        AudioBus::Create(audio_bus.channels(), audio_bus.frames()).Pass();
    audio_bus.CopyTo(expected_audio_frame->audio_bus.get());
    expected_audio_frame->record_time = record_time;
    expected_frames_.push_back(expected_audio_frame.release());
  }

  void IgnoreAudioFrame(scoped_ptr<AudioBus> audio_bus,
                        const base::TimeTicks& playout_time,
                        bool is_continuous) {
    ++num_called_;
  }

  void CheckAudioFrame(scoped_ptr<AudioBus> audio_bus,
                       const base::TimeTicks& playout_time,
                       bool is_continuous) {
    ++num_called_;

    ASSERT_TRUE(!!audio_bus);
    ASSERT_FALSE(expected_frames_.empty());
    const scoped_ptr<ExpectedAudioFrame> expected_audio_frame(
        expected_frames_.front());
    expected_frames_.pop_front();

    EXPECT_EQ(audio_bus->channels(), kAudioChannels);
    EXPECT_EQ(audio_bus->frames(), expected_audio_frame->audio_bus->frames());
    for (int ch = 0; ch < audio_bus->channels(); ++ch) {
      EXPECT_NEAR(CountZeroCrossings(
                      expected_audio_frame->audio_bus->channel(ch),
                      expected_audio_frame->audio_bus->frames()),
                  CountZeroCrossings(audio_bus->channel(ch),
                                     audio_bus->frames()),
                  1);
    }

    // TODO(miu): This is a "fuzzy" way to check the timestamps.  We should be
    // able to compute exact offsets with "omnipotent" knowledge of the system.
    const base::TimeTicks upper_bound =
        expected_audio_frame->record_time +
        base::TimeDelta::FromMilliseconds(kDefaultRtpMaxDelayMs +
                                          kTimerErrorMs);
    EXPECT_GE(upper_bound, playout_time)
        << "playout_time - upper_bound == "
        << (playout_time - upper_bound).InMicroseconds() << " usec";

    EXPECT_TRUE(is_continuous);
  }

  void CheckCodedAudioFrame(
      scoped_ptr<transport::EncodedAudioFrame> audio_frame,
      const base::TimeTicks& playout_time) {
    ASSERT_TRUE(!!audio_frame);
    ASSERT_FALSE(expected_frames_.empty());
    const ExpectedAudioFrame& expected_audio_frame =
        *(expected_frames_.front());
    // Note: Just peeking here.  Will delegate to CheckAudioFrame() to pop.

    // We need to "decode" the encoded audio frame.  The codec is simply to
    // swizzle the bytes of each int16 from host-->network-->host order to get
    // interleaved int16 PCM.  Then, make an AudioBus out of that.
    const int num_elements = audio_frame->data.size() / sizeof(int16);
    ASSERT_EQ(expected_audio_frame.audio_bus->channels() *
                  expected_audio_frame.audio_bus->frames(),
              num_elements);
    int16* const pcm_data =
        reinterpret_cast<int16*>(string_as_array(&audio_frame->data));
    for (int i = 0; i < num_elements; ++i)
      pcm_data[i] = static_cast<int16>(base::NetToHost16(pcm_data[i]));
    scoped_ptr<AudioBus> audio_bus(
        AudioBus::Create(expected_audio_frame.audio_bus->channels(),
                         expected_audio_frame.audio_bus->frames()));
    audio_bus->FromInterleaved(pcm_data, audio_bus->frames(), sizeof(int16));

    // Delegate the checking from here...
    CheckAudioFrame(audio_bus.Pass(), playout_time, true);
  }

  int number_times_called() const { return num_called_; }

 protected:
  virtual ~TestReceiverAudioCallback() {
    STLDeleteElements(&expected_frames_);
  }

 private:
  friend class base::RefCountedThreadSafe<TestReceiverAudioCallback>;

  int num_called_;
  int expected_sampling_frequency_;
  std::list<ExpectedAudioFrame*> expected_frames_;
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
    bool should_be_continuous;
  };

  TestReceiverVideoCallback() : num_called_(0) {}

  void AddExpectedResult(int start_value,
                         int width,
                         int height,
                         const base::TimeTicks& capture_time,
                         bool should_be_continuous) {
    ExpectedVideoFrame expected_video_frame;
    expected_video_frame.start_value = start_value;
    expected_video_frame.width = width;
    expected_video_frame.height = height;
    expected_video_frame.capture_time = capture_time;
    expected_video_frame.should_be_continuous = should_be_continuous;
    expected_frame_.push_back(expected_video_frame);
  }

  void CheckVideoFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                       const base::TimeTicks& render_time,
                       bool is_continuous) {
    ++num_called_;

    ASSERT_TRUE(!!video_frame);
    ASSERT_FALSE(expected_frame_.empty());
    ExpectedVideoFrame expected_video_frame = expected_frame_.front();
    expected_frame_.pop_front();

    base::TimeDelta time_since_capture =
        render_time - expected_video_frame.capture_time;
    const base::TimeDelta upper_bound = base::TimeDelta::FromMilliseconds(
        kDefaultRtpMaxDelayMs + kTimerErrorMs);

    // TODO(miu): This is a "fuzzy" way to check the timestamps.  We should be
    // able to compute exact offsets with "omnipotent" knowledge of the system.
    EXPECT_GE(upper_bound, time_since_capture)
        << "time_since_capture - upper_bound == "
        << (time_since_capture - upper_bound).InMicroseconds() << " usec";
    // TODO(miu): I broke the concept of 100 ms target delay timing on the
    // receiver side, but the logic for computing playout time really isn't any
    // more broken than it was.  This only affects the receiver, and is to be
    // rectified in an soon-upcoming change.  http://crbug.com/356942
    // EXPECT_LE(expected_video_frame.capture_time, render_time);
    EXPECT_EQ(expected_video_frame.width, video_frame->visible_rect().width());
    EXPECT_EQ(expected_video_frame.height,
              video_frame->visible_rect().height());

    gfx::Size size(expected_video_frame.width, expected_video_frame.height);
    scoped_refptr<media::VideoFrame> expected_I420_frame =
        media::VideoFrame::CreateFrame(
            VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(expected_I420_frame, expected_video_frame.start_value);

    EXPECT_GE(I420PSNR(expected_I420_frame, video_frame), kVideoAcceptedPSNR);

    EXPECT_EQ(expected_video_frame.should_be_continuous, is_continuous);
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
        testing_clock_sender_(new base::SimpleTestTickClock()),
        testing_clock_receiver_(new base::SimpleTestTickClock()),
        task_runner_(
            new test::FakeSingleThreadTaskRunner(testing_clock_sender_)),
        cast_environment_sender_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_sender_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_)),
        cast_environment_receiver_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_receiver_).Pass(),
            task_runner_,
            task_runner_,
            task_runner_)),
        receiver_to_sender_(cast_environment_receiver_),
        sender_to_receiver_(cast_environment_sender_),
        test_receiver_audio_callback_(new TestReceiverAudioCallback()),
        test_receiver_video_callback_(new TestReceiverVideoCallback()) {
    testing_clock_sender_->Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    testing_clock_receiver_->Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    cast_environment_sender_->Logging()->AddRawEventSubscriber(
        &event_subscriber_sender_);
  }

  void Configure(transport::AudioCodec audio_codec,
                 int audio_sampling_frequency,
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

    transport_audio_config_.base.ssrc = audio_sender_config_.sender_ssrc;
    transport_audio_config_.codec = audio_sender_config_.codec;
    transport_audio_config_.base.rtp_config = audio_sender_config_.rtp_config;
    transport_audio_config_.frequency = audio_sender_config_.frequency;
    transport_audio_config_.channels = audio_sender_config_.channels;
    transport_video_config_.base.ssrc = video_sender_config_.sender_ssrc;
    transport_video_config_.codec = video_sender_config_.codec;
    transport_video_config_.base.rtp_config = video_sender_config_.rtp_config;
  }

  void FeedAudioFrames(int count, bool will_be_checked) {
    for (int i = 0; i < count; ++i) {
      scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
          base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs)));
      const base::TimeTicks send_time =
          testing_clock_sender_->NowTicks() +
              i * base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs);
      if (will_be_checked)
        test_receiver_audio_callback_->AddExpectedResult(*audio_bus, send_time);
      audio_frame_input_->InsertAudio(audio_bus.Pass(), send_time);
    }
  }

  void FeedAudioFramesWithExpectedDelay(int count,
                                        const base::TimeDelta& delay) {
    for (int i = 0; i < count; ++i) {
      scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
          base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs)));
      const base::TimeTicks send_time =
          testing_clock_sender_->NowTicks() +
              i * base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs);
      test_receiver_audio_callback_->AddExpectedResult(*audio_bus,
                                                       send_time + delay);
      audio_frame_input_->InsertAudio(audio_bus.Pass(), send_time);
    }
  }

  void RequestAudioFrames(int count, bool with_check) {
    for (int i = 0; i < count; ++i) {
      frame_receiver_->GetRawAudioFrame(
          base::Bind(with_check ? &TestReceiverAudioCallback::CheckAudioFrame :
                                  &TestReceiverAudioCallback::IgnoreAudioFrame,
                     test_receiver_audio_callback_));
    }
  }

  void Create() {
    cast_receiver_ = CastReceiver::Create(cast_environment_receiver_,
                                          audio_receiver_config_,
                                          video_receiver_config_,
                                          &receiver_to_sender_);
    net::IPEndPoint dummy_endpoint;
    transport_sender_.reset(new transport::CastTransportSenderImpl(
        NULL,
        testing_clock_sender_,
        dummy_endpoint,
        base::Bind(&UpdateCastTransportStatus),
        base::Bind(&End2EndTest::LogRawEvents, base::Unretained(this)),
        base::TimeDelta::FromSeconds(1),
        task_runner_,
        &sender_to_receiver_));
    transport_sender_->InitializeAudio(transport_audio_config_);
    transport_sender_->InitializeVideo(transport_video_config_);

    cast_sender_ =
        CastSender::Create(cast_environment_sender_, transport_sender_.get());

    // Initializing audio and video senders.
    cast_sender_->InitializeAudio(audio_sender_config_,
                                  base::Bind(&AudioInitializationStatus));
    cast_sender_->InitializeVideo(video_sender_config_,
                                  base::Bind(&VideoInitializationStatus),
                                  CreateDefaultVideoEncodeAcceleratorCallback(),
                                  CreateDefaultVideoEncodeMemoryCallback());

    receiver_to_sender_.SetPacketReceiver(cast_sender_->packet_receiver());
    sender_to_receiver_.SetPacketReceiver(cast_receiver_->packet_receiver());

    audio_frame_input_ = cast_sender_->audio_frame_input();
    video_frame_input_ = cast_sender_->video_frame_input();

    frame_receiver_ = cast_receiver_->frame_receiver();

    audio_bus_factory_.reset(
        new TestAudioBusFactory(audio_sender_config_.channels,
                                audio_sender_config_.frequency,
                                kSoundFrequency,
                                kSoundVolume));
  }

  virtual ~End2EndTest() {
    cast_environment_sender_->Logging()->RemoveRawEventSubscriber(
        &event_subscriber_sender_);
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
    video_frame_input_->InsertRawVideoFrame(video_frame, capture_time);
  }

  void RunTasks(int during_ms) {
    for (int i = 0; i < during_ms; ++i) {
      // Call process the timers every 1 ms.
      testing_clock_sender_->Advance(base::TimeDelta::FromMilliseconds(1));
      testing_clock_receiver_->Advance(base::TimeDelta::FromMilliseconds(1));
      task_runner_->RunTasks();
    }
  }

  void LogRawEvents(const std::vector<PacketEvent>& packet_events) {
    EXPECT_FALSE(packet_events.empty());
    for (std::vector<media::cast::PacketEvent>::const_iterator it =
             packet_events.begin();
         it != packet_events.end();
         ++it) {
      cast_environment_sender_->Logging()->InsertPacketEvent(it->timestamp,
                                                             it->type,
                                                             it->rtp_timestamp,
                                                             it->frame_id,
                                                             it->packet_id,
                                                             it->max_packet_id,
                                                             it->size);
    }
  }

  AudioReceiverConfig audio_receiver_config_;
  VideoReceiverConfig video_receiver_config_;
  AudioSenderConfig audio_sender_config_;
  VideoSenderConfig video_sender_config_;
  transport::CastTransportAudioConfig transport_audio_config_;
  transport::CastTransportVideoConfig transport_video_config_;

  base::TimeTicks start_time_;
  base::SimpleTestTickClock* testing_clock_sender_;
  base::SimpleTestTickClock* testing_clock_receiver_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_sender_;
  scoped_refptr<CastEnvironment> cast_environment_receiver_;

  LoopBackTransport receiver_to_sender_;
  LoopBackTransport sender_to_receiver_;
  scoped_ptr<transport::CastTransportSenderImpl> transport_sender_;

  scoped_ptr<CastReceiver> cast_receiver_;
  scoped_ptr<CastSender> cast_sender_;
  scoped_refptr<AudioFrameInput> audio_frame_input_;
  scoped_refptr<VideoFrameInput> video_frame_input_;
  scoped_refptr<FrameReceiver> frame_receiver_;

  scoped_refptr<TestReceiverAudioCallback> test_receiver_audio_callback_;
  scoped_refptr<TestReceiverVideoCallback> test_receiver_video_callback_;

  scoped_ptr<TestAudioBusFactory> audio_bus_factory_;

  SimpleEventSubscriber event_subscriber_sender_;
  std::vector<FrameEvent> frame_events_;
  std::vector<PacketEvent> packet_events_;
  std::vector<GenericEvent> generic_events_;
  // |transport_sender_| has a RepeatingTimer which needs a MessageLoop.
  base::MessageLoop message_loop_;
};

TEST_F(End2EndTest, LoopNoLossPcm16) {
  Configure(transport::kPcm16, 32000, false, 1);
  // Reduce video resolution to allow processing multiple frames within a
  // reasonable time frame.
  video_sender_config_.width = kVideoQcifWidth;
  video_sender_config_.height = kVideoQcifHeight;
  Create();

  const int kNumIterations = 50;
  int video_start = kVideoStart;
  int audio_diff = kFrameTimerMs;
  int num_audio_frames_requested = 0;
  for (int i = 0; i < kNumIterations; ++i) {
    const int num_audio_frames = audio_diff / kAudioFrameDurationMs;
    audio_diff -= num_audio_frames * kAudioFrameDurationMs;

    if (num_audio_frames > 0)
      FeedAudioFrames(1, true);

    test_receiver_video_callback_->AddExpectedResult(
        video_start,
        video_sender_config_.width,
        video_sender_config_.height,
        testing_clock_sender_->NowTicks(),
        true);
    SendVideoFrame(video_start, testing_clock_sender_->NowTicks());

    if (num_audio_frames > 0)
      RunTasks(kAudioFrameDurationMs);  // Advance clock forward.
    if (num_audio_frames > 1)
      FeedAudioFrames(num_audio_frames - 1, true);

    RequestAudioFrames(num_audio_frames, true);
    num_audio_frames_requested += num_audio_frames;

    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_));

    RunTasks(kFrameTimerMs - kAudioFrameDurationMs);
    audio_diff += kFrameTimerMs;
    video_start++;
  }

  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(num_audio_frames_requested,
            test_receiver_audio_callback_->number_times_called());
  EXPECT_EQ(kNumIterations,
            test_receiver_video_callback_->number_times_called());
}

// This tests our external decoder interface for Audio.
// Audio test without packet loss using raw PCM 16 audio "codec";
TEST_F(End2EndTest, LoopNoLossPcm16ExternalDecoder) {
  Configure(transport::kPcm16, 32000, true, 1);
  Create();

  const int kNumIterations = 10;
  for (int i = 0; i < kNumIterations; ++i) {
    FeedAudioFrames(1, true);
    RunTasks(kAudioFrameDurationMs);
    frame_receiver_->GetCodedAudioFrame(
        base::Bind(&TestReceiverAudioCallback::CheckCodedAudioFrame,
                   test_receiver_audio_callback_));
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(kNumIterations,
            test_receiver_audio_callback_->number_times_called());
}

// This tests our Opus audio codec without video.
TEST_F(End2EndTest, LoopNoLossOpus) {
  Configure(transport::kOpus, kDefaultAudioSamplingRate, false, 1);
  Create();

  const int kNumIterations = 300;
  for (int i = 0; i < kNumIterations; ++i) {
    // Opus introduces a tiny delay before the sinewave starts; so don't examine
    // the first frame.
    const bool examine_audio_data = i > 0;
    FeedAudioFrames(1, examine_audio_data);
    RunTasks(kAudioFrameDurationMs);
    RequestAudioFrames(1, examine_audio_data);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(kNumIterations,
            test_receiver_audio_callback_->number_times_called());
}

// This tests start sending audio and video at start-up time before the receiver
// is ready; it sends 2 frames before the receiver comes online.
//
// Test disabled due to flakiness: It appears that the RTCP synchronization
// sometimes kicks in, and sometimes doesn't.  When it does, there's a sharp
// discontinuity in the timeline, throwing off the test expectations.  See TODOs
// in audio_receiver.cc for likely cause(s) of this bug.
// http://crbug.com/356942
TEST_F(End2EndTest, DISABLED_StartSenderBeforeReceiver) {
  Configure(transport::kPcm16, kDefaultAudioSamplingRate, false, 1);
  Create();

  int video_start = kVideoStart;
  int audio_diff = kFrameTimerMs;

  sender_to_receiver_.SetSendPackets(false);

  const int test_delay_ms = 100;

  const int kNumVideoFramesBeforeReceiverStarted = 2;
  const base::TimeTicks initial_send_time = testing_clock_sender_->NowTicks();
  const base::TimeDelta expected_delay =
      base::TimeDelta::FromMilliseconds(test_delay_ms + kFrameTimerMs);
  for (int i = 0; i < kNumVideoFramesBeforeReceiverStarted; ++i) {
    const int num_audio_frames = audio_diff / kAudioFrameDurationMs;
    audio_diff -= num_audio_frames * kAudioFrameDurationMs;

    if (num_audio_frames > 0)
      FeedAudioFramesWithExpectedDelay(1, expected_delay);

    // Frame will be rendered with 100mS delay, as the transmission is delayed.
    // The receiver at this point cannot be synced to the sender's clock, as no
    // packets, and specifically no RTCP packets were sent.
    test_receiver_video_callback_->AddExpectedResult(
        video_start,
        video_sender_config_.width,
        video_sender_config_.height,
        initial_send_time + expected_delay,
        true);
    SendVideoFrame(video_start, testing_clock_sender_->NowTicks());

    if (num_audio_frames > 0)
      RunTasks(kAudioFrameDurationMs);  // Advance clock forward.
    if (num_audio_frames > 1)
      FeedAudioFramesWithExpectedDelay(num_audio_frames - 1, expected_delay);

    RunTasks(kFrameTimerMs - kAudioFrameDurationMs);
    audio_diff += kFrameTimerMs;
    video_start++;
  }

  RunTasks(test_delay_ms);
  sender_to_receiver_.SetSendPackets(true);

  int num_audio_frames_requested = 0;
  for (int j = 0; j < 10; ++j) {
    const int num_audio_frames = audio_diff / kAudioFrameDurationMs;
    audio_diff -= num_audio_frames * kAudioFrameDurationMs;

    if (num_audio_frames > 0)
      FeedAudioFrames(1, true);

    test_receiver_video_callback_->AddExpectedResult(
        video_start,
        video_sender_config_.width,
        video_sender_config_.height,
        testing_clock_sender_->NowTicks(),
        true);
    SendVideoFrame(video_start, testing_clock_sender_->NowTicks());

    if (num_audio_frames > 0)
      RunTasks(kAudioFrameDurationMs);  // Advance clock forward.
    if (num_audio_frames > 1)
      FeedAudioFrames(num_audio_frames - 1, true);

    RequestAudioFrames(num_audio_frames, true);
    num_audio_frames_requested += num_audio_frames;

    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_));

    RunTasks(kFrameTimerMs - kAudioFrameDurationMs);
    audio_diff += kFrameTimerMs;
    video_start++;
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(num_audio_frames_requested,
            test_receiver_audio_callback_->number_times_called());
  EXPECT_EQ(10, test_receiver_video_callback_->number_times_called());
}

// This tests a network glitch lasting for 10 video frames.
// Flaky. See crbug.com/351596.
TEST_F(End2EndTest, DISABLED_GlitchWith3Buffers) {
  Configure(transport::kOpus, kDefaultAudioSamplingRate, false, 3);
  video_sender_config_.rtp_config.max_delay_ms = 67;
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();

  int video_start = kVideoStart;
  base::TimeTicks send_time;
  // Frames will rendered on completion until the render time stabilizes, i.e.
  // we got enough data.
  const int frames_before_glitch = 20;
  for (int i = 0; i < frames_before_glitch; ++i) {
    send_time = testing_clock_sender_->NowTicks();
    SendVideoFrame(video_start, send_time);
    test_receiver_video_callback_->AddExpectedResult(
        video_start,
        video_sender_config_.width,
        video_sender_config_.height,
        send_time,
        true);
    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_));
    RunTasks(kFrameTimerMs);
    video_start++;
  }

  // Introduce a glitch lasting for 10 frames.
  sender_to_receiver_.SetSendPackets(false);
  for (int i = 0; i < 10; ++i) {
    send_time = testing_clock_sender_->NowTicks();
    // First 3 will be sent and lost.
    SendVideoFrame(video_start, send_time);
    RunTasks(kFrameTimerMs);
    video_start++;
  }
  sender_to_receiver_.SetSendPackets(true);
  RunTasks(100);
  send_time = testing_clock_sender_->NowTicks();

  // Frame 1 should be acked by now and we should have an opening to send 4.
  SendVideoFrame(video_start, send_time);
  RunTasks(kFrameTimerMs);

  // Frames 1-3 are old frames by now, and therefore should be decoded, but
  // not rendered. The next frame we expect to render is frame #4.
  test_receiver_video_callback_->AddExpectedResult(video_start,
                                                   video_sender_config_.width,
                                                   video_sender_config_.height,
                                                   send_time,
                                                   true);

  frame_receiver_->GetRawVideoFrame(
      base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                 test_receiver_video_callback_));

  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(frames_before_glitch + 1,
            test_receiver_video_callback_->number_times_called());
}

// Disabled due to flakiness and crashiness.  http://crbug.com/360951
TEST_F(End2EndTest, DISABLED_DropEveryOtherFrame3Buffers) {
  Configure(transport::kOpus, kDefaultAudioSamplingRate, false, 3);
  video_sender_config_.rtp_config.max_delay_ms = 67;
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();
  sender_to_receiver_.DropAllPacketsBelongingToOddFrames();

  int video_start = kVideoStart;
  base::TimeTicks send_time;

  int i = 0;
  for (; i < 20; ++i) {
    send_time = testing_clock_sender_->NowTicks();
    SendVideoFrame(video_start, send_time);

    if (i % 2 == 0) {
      test_receiver_video_callback_->AddExpectedResult(
          video_start,
          video_sender_config_.width,
          video_sender_config_.height,
          send_time,
          i == 0);

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
  Configure(transport::kOpus, kDefaultAudioSamplingRate, false, 3);
  video_sender_config_.rtp_config.max_delay_ms = 67;
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();
  sender_to_receiver_.AlwaysResetReferenceFrameId();

  int frames_counter = 0;
  for (; frames_counter < 10; ++frames_counter) {
    const base::TimeTicks send_time = testing_clock_sender_->NowTicks();
    SendVideoFrame(frames_counter, send_time);

    test_receiver_video_callback_->AddExpectedResult(
        frames_counter,
        video_sender_config_.width,
        video_sender_config_.height,
        send_time,
        true);

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
  Configure(transport::kPcm16, 32000, false, 1);

  transport_video_config_.base.aes_iv_mask =
      ConvertFromBase16String("1234567890abcdeffedcba0987654321");
  transport_video_config_.base.aes_key =
      ConvertFromBase16String("deadbeefcafeb0b0b0b0cafedeadbeef");

  video_receiver_config_.aes_iv_mask = transport_video_config_.base.aes_iv_mask;
  video_receiver_config_.aes_key = transport_video_config_.base.aes_key;

  Create();

  int frames_counter = 0;
  for (; frames_counter < 3; ++frames_counter) {
    const base::TimeTicks send_time = testing_clock_sender_->NowTicks();
    SendVideoFrame(frames_counter, send_time);

    test_receiver_video_callback_->AddExpectedResult(
        frames_counter,
        video_sender_config_.width,
        video_sender_config_.height,
        send_time,
        true);

    RunTasks(kFrameTimerMs);

    frame_receiver_->GetRawVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_));
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(frames_counter,
            test_receiver_video_callback_->number_times_called());
}

TEST_F(End2EndTest, CryptoAudio) {
  Configure(transport::kPcm16, 32000, false, 1);

  transport_audio_config_.base.aes_iv_mask =
      ConvertFromBase16String("abcdeffedcba12345678900987654321");
  transport_audio_config_.base.aes_key =
      ConvertFromBase16String("deadbeefcafecafedeadbeefb0b0b0b0");

  audio_receiver_config_.aes_iv_mask = transport_audio_config_.base.aes_iv_mask;
  audio_receiver_config_.aes_key = transport_audio_config_.base.aes_key;

  Create();

  const int kNumIterations = 3;
  const int kNumAudioFramesPerIteration = 2;
  for (int i = 0; i < kNumIterations; ++i) {
    FeedAudioFrames(kNumAudioFramesPerIteration, true);
    RunTasks(kNumAudioFramesPerIteration * kAudioFrameDurationMs);
    RequestAudioFrames(kNumAudioFramesPerIteration, true);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(kNumIterations * kNumAudioFramesPerIteration,
            test_receiver_audio_callback_->number_times_called());
}

// Video test without packet loss - tests the logging aspects of the end2end,
// but is basically equivalent to LoopNoLossPcm16.
TEST_F(End2EndTest, VideoLogging) {
  Configure(transport::kPcm16, 32000, false, 1);
  Create();

  int video_start = kVideoStart;
  const int num_frames = 5;
  for (int i = 0; i < num_frames; ++i) {
    base::TimeTicks send_time = testing_clock_sender_->NowTicks();
    test_receiver_video_callback_->AddExpectedResult(
        video_start,
        video_sender_config_.width,
        video_sender_config_.height,
        send_time,
        true);

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
  event_subscriber_sender_.GetFrameEventsAndReset(&frame_events_);

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

    EXPECT_EQ(1, map_it->second.counter[kVideoFrameCaptured]);
    expected_event_count_for_frame +=
        map_it->second.counter[kVideoFrameCaptured];

    EXPECT_EQ(1, map_it->second.counter[kVideoFrameSentToEncoder]);
    expected_event_count_for_frame +=
        map_it->second.counter[kVideoFrameSentToEncoder];

    EXPECT_EQ(1, map_it->second.counter[kVideoFrameEncoded]);
    expected_event_count_for_frame +=
        map_it->second.counter[kVideoFrameEncoded];

    EXPECT_EQ(1, map_it->second.counter[kVideoFrameReceived]);
    expected_event_count_for_frame +=
        map_it->second.counter[kVideoFrameReceived];

    EXPECT_EQ(1, map_it->second.counter[kVideoRenderDelay]);
    expected_event_count_for_frame += map_it->second.counter[kVideoRenderDelay];

    EXPECT_EQ(1, map_it->second.counter[kVideoFrameDecoded]);
    expected_event_count_for_frame +=
        map_it->second.counter[kVideoFrameDecoded];

    // There is no guarantee that kVideoAckSent is loggeed exactly once per
    // frame.
    EXPECT_GT(map_it->second.counter[kVideoAckSent], 0);
    expected_event_count_for_frame += map_it->second.counter[kVideoAckSent];

    // There is no guarantee that kVideoAckReceived is loggeed exactly once per
    // frame.
    EXPECT_GT(map_it->second.counter[kVideoAckReceived], 0);
    expected_event_count_for_frame += map_it->second.counter[kVideoAckReceived];

    // Verify that there were no other events logged with respect to this
    // frame.
    // (i.e. Total event count = expected event count)
    EXPECT_EQ(total_event_count_for_frame, expected_event_count_for_frame);
  }

  // Packet logging.
  // Verify that all packet related events were logged.
  event_subscriber_sender_.GetPacketEventsAndReset(&packet_events_);
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

// Audio test without packet loss - tests the logging aspects of the end2end,
// but is basically equivalent to LoopNoLossPcm16.
TEST_F(End2EndTest, AudioLogging) {
  Configure(transport::kPcm16, 32000, false, 1);
  Create();

  int audio_diff = kFrameTimerMs;
  const int kNumVideoFrames = 10;
  int num_audio_frames_requested = 0;
  for (int i = 0; i < kNumVideoFrames; ++i) {
    const int num_audio_frames = audio_diff / kAudioFrameDurationMs;
    audio_diff -= num_audio_frames * kAudioFrameDurationMs;

    FeedAudioFrames(num_audio_frames, true);

    RunTasks(kFrameTimerMs);
    audio_diff += kFrameTimerMs;

    RequestAudioFrames(num_audio_frames, true);
    num_audio_frames_requested += num_audio_frames;
  }

  // Basic tests.
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.

  EXPECT_EQ(num_audio_frames_requested,
            test_receiver_audio_callback_->number_times_called());

  // Logging tests.
  // Verify that all frames and all required events were logged.
  event_subscriber_sender_.GetFrameEventsAndReset(&frame_events_);

  // Construct a map from each frame (RTP timestamp) to a count of each event
  // type logged for that frame.
  std::map<RtpTimestamp, LoggingEventCounts> event_counter_for_frame =
      GetEventCountForFrameEvents(frame_events_);

  int received_count = 0;
  int encoded_count = 0;

  // Verify the right number of events were logged for each event type.
  for (std::map<RtpTimestamp, LoggingEventCounts>::iterator it =
           event_counter_for_frame.begin();
       it != event_counter_for_frame.end();
       ++it) {
    received_count += it->second.counter[kAudioFrameReceived];
    encoded_count += it->second.counter[kAudioFrameEncoded];
  }

  EXPECT_EQ(num_audio_frames_requested, received_count);
  EXPECT_EQ(num_audio_frames_requested, encoded_count);

  std::map<RtpTimestamp, LoggingEventCounts>::iterator map_it =
      event_counter_for_frame.begin();

  // Verify that each frame have the expected types of events logged.
  // TODO(imcheng): This only checks the first frame. This doesn't work
  // properly for all frames because:
  // 1. kAudioPlayoutDelay and kAudioFrameDecoded RTP timestamps aren't
  // exactly aligned with those of kAudioFrameReceived and kAudioFrameEncoded.
  // Note that these RTP timestamps are output from webrtc::AudioCodingModule
  // which are different from RTP timestamps that the cast library generates
  // during the encode step (and which are sent to receiver). The first frame
  // just happen to be aligned.
  // 2. Currently, kAudioFrameDecoded and kAudioPlayoutDelay are logged per
  // audio bus.
  // Both 1 and 2 may change since we are currently refactoring audio_decoder.
  // 3. There is no guarantee that exactly one kAudioAckSent is sent per frame.
  int total_event_count_for_frame = 0;
  for (int j = 0; j < kNumOfLoggingEvents; ++j)
    total_event_count_for_frame += map_it->second.counter[j];

  int expected_event_count_for_frame = 0;

  EXPECT_EQ(1, map_it->second.counter[kAudioFrameReceived]);
  expected_event_count_for_frame += map_it->second.counter[kAudioFrameReceived];

  EXPECT_EQ(1, map_it->second.counter[kAudioFrameEncoded]);
  expected_event_count_for_frame += map_it->second.counter[kAudioFrameEncoded];

  EXPECT_EQ(1, map_it->second.counter[kAudioPlayoutDelay]);
  expected_event_count_for_frame += map_it->second.counter[kAudioPlayoutDelay];

  EXPECT_EQ(1, map_it->second.counter[kAudioFrameDecoded]);
  expected_event_count_for_frame += map_it->second.counter[kAudioFrameDecoded];

  EXPECT_GT(map_it->second.counter[kAudioAckSent], 0);
  expected_event_count_for_frame += map_it->second.counter[kAudioAckSent];

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
