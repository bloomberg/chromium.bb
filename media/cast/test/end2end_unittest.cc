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
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/cast_transport_sender.h"
#include "media/cast/net/cast_transport_sender_impl.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/skewed_single_thread_task_runner.h"
#include "media/cast/test/skewed_tick_clock.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/udp_proxy.h"
#include "media/cast/test/utility/video_utility.h"
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

// Since the video encoded and decoded an error will be introduced; when
// comparing individual pixels the error can be quite large; we allow a PSNR of
// at least |kVideoAcceptedPSNR|.
static const double kVideoAcceptedPSNR = 38.0;

// The tests are commonly implemented with |kFrameTimerMs| RunTask function;
// a normal video is 30 fps hence the 33 ms between frames.
//
// TODO(miu): The errors in timing will add up significantly.  Find an
// alternative approach that eliminates use of this constant.
static const int kFrameTimerMs = 33;

// Start the video synthetic start value to medium range value, to avoid edge
// effects cause by encoding and quantization.
static const int kVideoStart = 100;

// The size of audio frames.  The encoder joins/breaks all inserted audio into
// chunks of this size.
static const int kAudioFrameDurationMs = 10;

// The amount of time between frame capture on the sender and playout on the
// receiver.
static const int kTargetPlayoutDelayMs = 100;

// The maximum amount of deviation expected in the playout times emitted by the
// receiver.
static const int kMaxAllowedPlayoutErrorMs = 30;

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

void UpdateCastTransportStatus(CastTransportStatus status) {
  bool result = (status == TRANSPORT_AUDIO_INITIALIZED ||
                 status == TRANSPORT_VIDEO_INITIALIZED);
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
  int counter[kNumOfLoggingEvents+1];
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

// Shim that turns forwards packets from a test::PacketPipe to a
// PacketReceiverCallback.
class LoopBackPacketPipe : public test::PacketPipe {
 public:
  LoopBackPacketPipe(const PacketReceiverCallback& packet_receiver)
      : packet_receiver_(packet_receiver) {}

  virtual ~LoopBackPacketPipe() {}

  // PacketPipe implementations.
  virtual void Send(scoped_ptr<Packet> packet) OVERRIDE {
    packet_receiver_.Run(packet.Pass());
  }

 private:
  PacketReceiverCallback packet_receiver_;
};

// Class that sends the packet direct from sender into the receiver with the
// ability to drop packets between the two.
class LoopBackTransport : public PacketSender {
 public:
  explicit LoopBackTransport(scoped_refptr<CastEnvironment> cast_environment)
      : send_packets_(true),
        drop_packets_belonging_to_odd_frames_(false),
        cast_environment_(cast_environment),
        bytes_sent_(0) {}

  void SetPacketReceiver(
      const PacketReceiverCallback& packet_receiver,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::TickClock* clock) {
    scoped_ptr<test::PacketPipe> loopback_pipe(
        new LoopBackPacketPipe(packet_receiver));
    if (packet_pipe_) {
      packet_pipe_->AppendToPipe(loopback_pipe.Pass());
    } else {
      packet_pipe_ = loopback_pipe.Pass();
    }
    packet_pipe_->InitOnIOThread(task_runner, clock);
  }

  virtual bool SendPacket(PacketRef packet,
                          const base::Closure& cb) OVERRIDE {
    DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
    if (!send_packets_)
      return false;

    bytes_sent_ += packet->data.size();
    if (drop_packets_belonging_to_odd_frames_) {
      uint32 frame_id = packet->data[13];
      if (frame_id % 2 == 1)
        return true;
    }

    scoped_ptr<Packet> packet_copy(new Packet(packet->data));
    packet_pipe_->Send(packet_copy.Pass());
    return true;
  }

  virtual int64 GetBytesSent() OVERRIDE {
    return bytes_sent_;
  }

  void SetSendPackets(bool send_packets) { send_packets_ = send_packets; }

  void DropAllPacketsBelongingToOddFrames() {
    drop_packets_belonging_to_odd_frames_ = true;
  }

  void SetPacketPipe(scoped_ptr<test::PacketPipe> pipe) {
    // Append the loopback pipe to the end.
    pipe->AppendToPipe(packet_pipe_.Pass());
    packet_pipe_ = pipe.Pass();
  }

 private:
  bool send_packets_;
  bool drop_packets_belonging_to_odd_frames_;
  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<test::PacketPipe> packet_pipe_;
  int64 bytes_sent_;
};

// Class that verifies the audio frames coming out of the receiver.
class TestReceiverAudioCallback
    : public base::RefCountedThreadSafe<TestReceiverAudioCallback> {
 public:
  struct ExpectedAudioFrame {
    scoped_ptr<AudioBus> audio_bus;
    base::TimeTicks playout_time;
  };

  TestReceiverAudioCallback() : num_called_(0) {}

  void SetExpectedSamplingFrequency(int expected_sampling_frequency) {
    expected_sampling_frequency_ = expected_sampling_frequency;
  }

  void AddExpectedResult(const AudioBus& audio_bus,
                         const base::TimeTicks& playout_time) {
    scoped_ptr<ExpectedAudioFrame> expected_audio_frame(
        new ExpectedAudioFrame());
    expected_audio_frame->audio_bus =
        AudioBus::Create(audio_bus.channels(), audio_bus.frames()).Pass();
    audio_bus.CopyTo(expected_audio_frame->audio_bus.get());
    expected_audio_frame->playout_time = playout_time;
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

    EXPECT_NEAR(
        (playout_time - expected_audio_frame->playout_time).InMillisecondsF(),
        0.0,
        kMaxAllowedPlayoutErrorMs);
    VLOG_IF(1, !last_playout_time_.is_null())
        << "Audio frame playout time delta (compared to last frame) is "
        << (playout_time - last_playout_time_).InMicroseconds() << " usec.";
    last_playout_time_ = playout_time;

    EXPECT_TRUE(is_continuous);
  }

  void CheckCodedAudioFrame(scoped_ptr<EncodedFrame> audio_frame) {
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
        reinterpret_cast<int16*>(audio_frame->mutable_bytes());
    for (int i = 0; i < num_elements; ++i)
      pcm_data[i] = static_cast<int16>(base::NetToHost16(pcm_data[i]));
    scoped_ptr<AudioBus> audio_bus(
        AudioBus::Create(expected_audio_frame.audio_bus->channels(),
                         expected_audio_frame.audio_bus->frames()));
    audio_bus->FromInterleaved(pcm_data, audio_bus->frames(), sizeof(int16));

    // Delegate the checking from here...
    CheckAudioFrame(audio_bus.Pass(), audio_frame->reference_time, true);
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
  base::TimeTicks last_playout_time_;
};

// Class that verifies the video frames coming out of the receiver.
class TestReceiverVideoCallback
    : public base::RefCountedThreadSafe<TestReceiverVideoCallback> {
 public:
  struct ExpectedVideoFrame {
    int start_value;
    int width;
    int height;
    base::TimeTicks playout_time;
    bool should_be_continuous;
  };

  TestReceiverVideoCallback() : num_called_(0) {}

  void AddExpectedResult(int start_value,
                         int width,
                         int height,
                         const base::TimeTicks& playout_time,
                         bool should_be_continuous) {
    ExpectedVideoFrame expected_video_frame;
    expected_video_frame.start_value = start_value;
    expected_video_frame.width = width;
    expected_video_frame.height = height;
    expected_video_frame.playout_time = playout_time;
    expected_video_frame.should_be_continuous = should_be_continuous;
    expected_frame_.push_back(expected_video_frame);
  }

  void CheckVideoFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                       const base::TimeTicks& playout_time,
                       bool is_continuous) {
    ++num_called_;

    ASSERT_TRUE(!!video_frame);
    ASSERT_FALSE(expected_frame_.empty());
    ExpectedVideoFrame expected_video_frame = expected_frame_.front();
    expected_frame_.pop_front();

    EXPECT_EQ(expected_video_frame.width, video_frame->visible_rect().width());
    EXPECT_EQ(expected_video_frame.height,
              video_frame->visible_rect().height());

    gfx::Size size(expected_video_frame.width, expected_video_frame.height);
    scoped_refptr<media::VideoFrame> expected_I420_frame =
        media::VideoFrame::CreateFrame(
            VideoFrame::I420, size, gfx::Rect(size), size, base::TimeDelta());
    PopulateVideoFrame(expected_I420_frame, expected_video_frame.start_value);

    EXPECT_GE(I420PSNR(expected_I420_frame, video_frame), kVideoAcceptedPSNR);

    EXPECT_NEAR(
        (playout_time - expected_video_frame.playout_time).InMillisecondsF(),
        0.0,
        kMaxAllowedPlayoutErrorMs);
    VLOG_IF(1, !last_playout_time_.is_null())
        << "Video frame playout time delta (compared to last frame) is "
        << (playout_time - last_playout_time_).InMicroseconds() << " usec.";
    last_playout_time_ = playout_time;

    EXPECT_EQ(expected_video_frame.should_be_continuous, is_continuous);
  }

  int number_times_called() const { return num_called_; }

 protected:
  virtual ~TestReceiverVideoCallback() {}

 private:
  friend class base::RefCountedThreadSafe<TestReceiverVideoCallback>;

  int num_called_;
  std::list<ExpectedVideoFrame> expected_frame_;
  base::TimeTicks last_playout_time_;
};

// The actual test class, generate synthetic data for both audio and video and
// send those through the sender and receiver and analyzes the result.
class End2EndTest : public ::testing::Test {
 protected:
  End2EndTest()
      : start_time_(),
        task_runner_(new test::FakeSingleThreadTaskRunner(&testing_clock_)),
        testing_clock_sender_(new test::SkewedTickClock(&testing_clock_)),
        task_runner_sender_(
            new test::SkewedSingleThreadTaskRunner(task_runner_)),
        testing_clock_receiver_(new test::SkewedTickClock(&testing_clock_)),
        task_runner_receiver_(
            new test::SkewedSingleThreadTaskRunner(task_runner_)),
        cast_environment_sender_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_sender_).Pass(),
            task_runner_sender_,
            task_runner_sender_,
            task_runner_sender_)),
        cast_environment_receiver_(new CastEnvironment(
            scoped_ptr<base::TickClock>(testing_clock_receiver_).Pass(),
            task_runner_receiver_,
            task_runner_receiver_,
            task_runner_receiver_)),
        receiver_to_sender_(cast_environment_receiver_),
        sender_to_receiver_(cast_environment_sender_),
        test_receiver_audio_callback_(new TestReceiverAudioCallback()),
        test_receiver_video_callback_(new TestReceiverVideoCallback()) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    cast_environment_sender_->Logging()->AddRawEventSubscriber(
        &event_subscriber_sender_);
  }

  void Configure(Codec video_codec,
                 Codec audio_codec,
                 int audio_sampling_frequency,
                 int max_number_of_video_buffers_used) {
    audio_sender_config_.ssrc = 1;
    audio_sender_config_.incoming_feedback_ssrc = 2;
    audio_sender_config_.target_playout_delay =
        base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs);
    audio_sender_config_.rtp_payload_type = 96;
    audio_sender_config_.use_external_encoder = false;
    audio_sender_config_.frequency = audio_sampling_frequency;
    audio_sender_config_.channels = kAudioChannels;
    audio_sender_config_.bitrate = kDefaultAudioEncoderBitrate;
    audio_sender_config_.codec = audio_codec;

    audio_receiver_config_.feedback_ssrc =
        audio_sender_config_.incoming_feedback_ssrc;
    audio_receiver_config_.incoming_ssrc = audio_sender_config_.ssrc;
    audio_receiver_config_.rtp_max_delay_ms = kTargetPlayoutDelayMs;
    audio_receiver_config_.rtp_payload_type =
        audio_sender_config_.rtp_payload_type;
    audio_receiver_config_.frequency = audio_sender_config_.frequency;
    audio_receiver_config_.channels = kAudioChannels;
    audio_receiver_config_.max_frame_rate = 100;
    audio_receiver_config_.codec = audio_sender_config_.codec;

    test_receiver_audio_callback_->SetExpectedSamplingFrequency(
        audio_receiver_config_.frequency);

    video_sender_config_.ssrc = 3;
    video_sender_config_.incoming_feedback_ssrc = 4;
    video_sender_config_.target_playout_delay =
        base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs);
    video_sender_config_.rtp_payload_type = 97;
    video_sender_config_.use_external_encoder = false;
    video_sender_config_.width = kVideoHdWidth;
    video_sender_config_.height = kVideoHdHeight;
    video_sender_config_.max_bitrate = 50000;
    video_sender_config_.min_bitrate = 10000;
    video_sender_config_.start_bitrate = 10000;
    video_sender_config_.max_qp = 30;
    video_sender_config_.min_qp = 4;
    video_sender_config_.max_frame_rate = 30;
    video_sender_config_.max_number_of_video_buffers_used =
        max_number_of_video_buffers_used;
    video_sender_config_.codec = video_codec;

    video_receiver_config_.feedback_ssrc =
        video_sender_config_.incoming_feedback_ssrc;
    video_receiver_config_.incoming_ssrc = video_sender_config_.ssrc;
    video_receiver_config_.rtp_max_delay_ms = kTargetPlayoutDelayMs;
    video_receiver_config_.rtp_payload_type =
        video_sender_config_.rtp_payload_type;
    video_receiver_config_.frequency = kVideoFrequency;
    video_receiver_config_.channels = 1;
    video_receiver_config_.max_frame_rate = video_sender_config_.max_frame_rate;
    video_receiver_config_.codec = video_sender_config_.codec;
  }

  void SetReceiverSkew(double skew, base::TimeDelta offset) {
    testing_clock_receiver_->SetSkew(skew, offset);
    task_runner_receiver_->SetSkew(1.0 / skew);
  }

  // Specify the minimum/maximum difference in playout times between two
  // consecutive frames.  Also, specify the maximum absolute rate of change over
  // each three consecutive frames.
  void SetExpectedVideoPlayoutSmoothness(base::TimeDelta min_delta,
                                         base::TimeDelta max_delta,
                                         base::TimeDelta max_curvature) {
    min_video_playout_delta_ = min_delta;
    max_video_playout_delta_ = max_delta;
    max_video_playout_curvature_ = max_curvature;
  }

  void FeedAudioFrames(int count, bool will_be_checked) {
    for (int i = 0; i < count; ++i) {
      scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
          base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs)));
      const base::TimeTicks capture_time =
          testing_clock_sender_->NowTicks() +
              i * base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs);
      if (will_be_checked) {
        test_receiver_audio_callback_->AddExpectedResult(
            *audio_bus,
            capture_time +
                base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs));
      }
      audio_frame_input_->InsertAudio(audio_bus.Pass(), capture_time);
    }
  }

  void FeedAudioFramesWithExpectedDelay(int count,
                                        const base::TimeDelta& delay) {
    for (int i = 0; i < count; ++i) {
      scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
          base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs)));
      const base::TimeTicks capture_time =
          testing_clock_sender_->NowTicks() +
              i * base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs);
      test_receiver_audio_callback_->AddExpectedResult(
          *audio_bus,
          capture_time + delay +
              base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs));
      audio_frame_input_->InsertAudio(audio_bus.Pass(), capture_time);
    }
  }

  void RequestAudioFrames(int count, bool with_check) {
    for (int i = 0; i < count; ++i) {
      cast_receiver_->RequestDecodedAudioFrame(
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
    transport_sender_.reset(new CastTransportSenderImpl(
        NULL,
        testing_clock_sender_,
        dummy_endpoint,
        base::Bind(&UpdateCastTransportStatus),
        base::Bind(&End2EndTest::LogRawEvents, base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(1),
        task_runner_sender_,
        &sender_to_receiver_));

    cast_sender_ =
        CastSender::Create(cast_environment_sender_, transport_sender_.get());

    // Initializing audio and video senders.
    cast_sender_->InitializeAudio(audio_sender_config_,
                                  base::Bind(&AudioInitializationStatus));
    cast_sender_->InitializeVideo(video_sender_config_,
                                  base::Bind(&VideoInitializationStatus),
                                  CreateDefaultVideoEncodeAcceleratorCallback(),
                                  CreateDefaultVideoEncodeMemoryCallback());

    receiver_to_sender_.SetPacketReceiver(
        transport_sender_->PacketReceiverForTesting(),
        task_runner_,
        &testing_clock_);
    sender_to_receiver_.SetPacketReceiver(cast_receiver_->packet_receiver(),
                                          task_runner_,
                                          &testing_clock_);

    audio_frame_input_ = cast_sender_->audio_frame_input();
    video_frame_input_ = cast_sender_->video_frame_input();

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

  void SendFakeVideoFrame(const base::TimeTicks& capture_time) {
    video_frame_input_->InsertRawVideoFrame(
        media::VideoFrame::CreateBlackFrame(gfx::Size(2, 2)), capture_time);
  }

  void RunTasks(int ms) {
    task_runner_->Sleep(base::TimeDelta::FromMilliseconds(ms));
  }

  void BasicPlayerGotVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& playout_time, bool continuous) {
    // The following tests that the sender and receiver clocks can be
    // out-of-sync, drift, and jitter with respect to one another; and depsite
    // this, the receiver will produce smoothly-progressing playout times.
    // Both first-order and second-order effects are tested.
    if (!last_video_playout_time_.is_null() &&
        min_video_playout_delta_ > base::TimeDelta()) {
      const base::TimeDelta delta = playout_time - last_video_playout_time_;
      VLOG(1) << "Video frame playout time delta (compared to last frame) is "
              << delta.InMicroseconds() << " usec.";
      EXPECT_LE(min_video_playout_delta_.InMicroseconds(),
                delta.InMicroseconds());
      EXPECT_GE(max_video_playout_delta_.InMicroseconds(),
                delta.InMicroseconds());
      if (last_video_playout_delta_ > base::TimeDelta()) {
        base::TimeDelta abs_curvature = delta - last_video_playout_delta_;
        if (abs_curvature < base::TimeDelta())
          abs_curvature = -abs_curvature;
        EXPECT_GE(max_video_playout_curvature_.InMicroseconds(),
                  abs_curvature.InMicroseconds());
      }
      last_video_playout_delta_ = delta;
    }
    last_video_playout_time_ = playout_time;

    video_ticks_.push_back(std::make_pair(
        testing_clock_receiver_->NowTicks(),
        playout_time));
    cast_receiver_->RequestDecodedVideoFrame(
        base::Bind(&End2EndTest::BasicPlayerGotVideoFrame,
                   base::Unretained(this)));
  }

  void BasicPlayerGotAudioFrame(scoped_ptr<AudioBus> audio_bus,
                                const base::TimeTicks& playout_time,
                                bool is_continuous) {
    VLOG_IF(1, !last_audio_playout_time_.is_null())
        << "Audio frame playout time delta (compared to last frame) is "
        << (playout_time - last_audio_playout_time_).InMicroseconds()
        << " usec.";
    last_audio_playout_time_ = playout_time;

    audio_ticks_.push_back(std::make_pair(
        testing_clock_receiver_->NowTicks(),
        playout_time));
    cast_receiver_->RequestDecodedAudioFrame(
        base::Bind(&End2EndTest::BasicPlayerGotAudioFrame,
                   base::Unretained(this)));
  }

  void StartBasicPlayer() {
    cast_receiver_->RequestDecodedVideoFrame(
        base::Bind(&End2EndTest::BasicPlayerGotVideoFrame,
                   base::Unretained(this)));
    cast_receiver_->RequestDecodedAudioFrame(
        base::Bind(&End2EndTest::BasicPlayerGotAudioFrame,
                   base::Unretained(this)));
  }

  void LogRawEvents(const std::vector<PacketEvent>& packet_events,
                    const std::vector<FrameEvent>& frame_events) {
    for (std::vector<media::cast::PacketEvent>::const_iterator it =
             packet_events.begin();
         it != packet_events.end();
         ++it) {
      cast_environment_sender_->Logging()->InsertPacketEvent(it->timestamp,
                                                             it->type,
                                                             it->media_type,
                                                             it->rtp_timestamp,
                                                             it->frame_id,
                                                             it->packet_id,
                                                             it->max_packet_id,
                                                             it->size);
    }
    for (std::vector<media::cast::FrameEvent>::const_iterator it =
             frame_events.begin();
         it != frame_events.end();
         ++it) {
      cast_environment_sender_->Logging()->InsertFrameEvent(it->timestamp,
                                                            it->type,
                                                            it->media_type,
                                                            it->rtp_timestamp,
                                                            it->frame_id);
    }
  }

  FrameReceiverConfig audio_receiver_config_;
  FrameReceiverConfig video_receiver_config_;
  AudioSenderConfig audio_sender_config_;
  VideoSenderConfig video_sender_config_;

  base::TimeTicks start_time_;

  // These run in "test time"
  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;

  // These run on the sender timeline.
  test::SkewedTickClock* testing_clock_sender_;
  scoped_refptr<test::SkewedSingleThreadTaskRunner> task_runner_sender_;

  // These run on the receiver timeline.
  test::SkewedTickClock* testing_clock_receiver_;
  scoped_refptr<test::SkewedSingleThreadTaskRunner> task_runner_receiver_;
  base::TimeDelta min_video_playout_delta_;
  base::TimeDelta max_video_playout_delta_;
  base::TimeDelta max_video_playout_curvature_;
  base::TimeTicks last_video_playout_time_;
  base::TimeDelta last_video_playout_delta_;
  base::TimeTicks last_audio_playout_time_;

  scoped_refptr<CastEnvironment> cast_environment_sender_;
  scoped_refptr<CastEnvironment> cast_environment_receiver_;

  LoopBackTransport receiver_to_sender_;
  LoopBackTransport sender_to_receiver_;
  scoped_ptr<CastTransportSenderImpl> transport_sender_;

  scoped_ptr<CastReceiver> cast_receiver_;
  scoped_ptr<CastSender> cast_sender_;
  scoped_refptr<AudioFrameInput> audio_frame_input_;
  scoped_refptr<VideoFrameInput> video_frame_input_;

  scoped_refptr<TestReceiverAudioCallback> test_receiver_audio_callback_;
  scoped_refptr<TestReceiverVideoCallback> test_receiver_video_callback_;

  scoped_ptr<TestAudioBusFactory> audio_bus_factory_;

  SimpleEventSubscriber event_subscriber_sender_;
  std::vector<FrameEvent> frame_events_;
  std::vector<PacketEvent> packet_events_;
  std::vector<std::pair<base::TimeTicks, base::TimeTicks> > audio_ticks_;
  std::vector<std::pair<base::TimeTicks, base::TimeTicks> > video_ticks_;
  // |transport_sender_| has a RepeatingTimer which needs a MessageLoop.
  base::MessageLoop message_loop_;
};

TEST_F(End2EndTest, LoopNoLossPcm16) {
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_PCM16, 32000, 1);
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
        testing_clock_sender_->NowTicks() +
            base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
        true);
    SendVideoFrame(video_start, testing_clock_sender_->NowTicks());

    if (num_audio_frames > 0)
      RunTasks(kAudioFrameDurationMs);  // Advance clock forward.
    if (num_audio_frames > 1)
      FeedAudioFrames(num_audio_frames - 1, true);

    RequestAudioFrames(num_audio_frames, true);
    num_audio_frames_requested += num_audio_frames;

    cast_receiver_->RequestDecodedVideoFrame(
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
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_PCM16, 32000, 1);
  Create();

  const int kNumIterations = 10;
  for (int i = 0; i < kNumIterations; ++i) {
    FeedAudioFrames(1, true);
    RunTasks(kAudioFrameDurationMs);
    cast_receiver_->RequestEncodedAudioFrame(
        base::Bind(&TestReceiverAudioCallback::CheckCodedAudioFrame,
                   test_receiver_audio_callback_));
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(kNumIterations,
            test_receiver_audio_callback_->number_times_called());
}

// This tests our Opus audio codec without video.
TEST_F(End2EndTest, LoopNoLossOpus) {
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_OPUS,
            kDefaultAudioSamplingRate, 1);
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
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_PCM16,
            kDefaultAudioSamplingRate, 1);
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
        initial_send_time + expected_delay +
            base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
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
        testing_clock_sender_->NowTicks() +
            base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
        true);
    SendVideoFrame(video_start, testing_clock_sender_->NowTicks());

    if (num_audio_frames > 0)
      RunTasks(kAudioFrameDurationMs);  // Advance clock forward.
    if (num_audio_frames > 1)
      FeedAudioFrames(num_audio_frames - 1, true);

    RequestAudioFrames(num_audio_frames, true);
    num_audio_frames_requested += num_audio_frames;

    cast_receiver_->RequestDecodedVideoFrame(
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
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_OPUS,
            kDefaultAudioSamplingRate, 3);
  video_sender_config_.target_playout_delay =
      base::TimeDelta::FromMilliseconds(67);
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();

  int video_start = kVideoStart;
  base::TimeTicks capture_time;
  // Frames will rendered on completion until the render time stabilizes, i.e.
  // we got enough data.
  const int frames_before_glitch = 20;
  for (int i = 0; i < frames_before_glitch; ++i) {
    capture_time = testing_clock_sender_->NowTicks();
    SendVideoFrame(video_start, capture_time);
    test_receiver_video_callback_->AddExpectedResult(
        video_start,
        video_sender_config_.width,
        video_sender_config_.height,
        capture_time + base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
        true);
    cast_receiver_->RequestDecodedVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_));
    RunTasks(kFrameTimerMs);
    video_start++;
  }

  // Introduce a glitch lasting for 10 frames.
  sender_to_receiver_.SetSendPackets(false);
  for (int i = 0; i < 10; ++i) {
    capture_time = testing_clock_sender_->NowTicks();
    // First 3 will be sent and lost.
    SendVideoFrame(video_start, capture_time);
    RunTasks(kFrameTimerMs);
    video_start++;
  }
  sender_to_receiver_.SetSendPackets(true);
  RunTasks(100);
  capture_time = testing_clock_sender_->NowTicks();

  // Frame 1 should be acked by now and we should have an opening to send 4.
  SendVideoFrame(video_start, capture_time);
  RunTasks(kFrameTimerMs);

  // Frames 1-3 are old frames by now, and therefore should be decoded, but
  // not rendered. The next frame we expect to render is frame #4.
  test_receiver_video_callback_->AddExpectedResult(
      video_start,
      video_sender_config_.width,
      video_sender_config_.height,
      capture_time + base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
      true);

  cast_receiver_->RequestDecodedVideoFrame(
      base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                 test_receiver_video_callback_));

  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(frames_before_glitch + 1,
            test_receiver_video_callback_->number_times_called());
}

// Disabled due to flakiness and crashiness.  http://crbug.com/360951
TEST_F(End2EndTest, DISABLED_DropEveryOtherFrame3Buffers) {
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_OPUS,
            kDefaultAudioSamplingRate, 3);
  video_sender_config_.target_playout_delay =
      base::TimeDelta::FromMilliseconds(67);
  video_receiver_config_.rtp_max_delay_ms = 67;
  Create();
  sender_to_receiver_.DropAllPacketsBelongingToOddFrames();

  int video_start = kVideoStart;
  base::TimeTicks capture_time;

  int i = 0;
  for (; i < 20; ++i) {
    capture_time = testing_clock_sender_->NowTicks();
    SendVideoFrame(video_start, capture_time);

    if (i % 2 == 0) {
      test_receiver_video_callback_->AddExpectedResult(
          video_start,
          video_sender_config_.width,
          video_sender_config_.height,
          capture_time +
              base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
          i == 0);

      // GetRawVideoFrame will not return the frame until we are close in
      // time before we should render the frame.
      cast_receiver_->RequestDecodedVideoFrame(
          base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                     test_receiver_video_callback_));
    }
    RunTasks(kFrameTimerMs);
    video_start++;
  }

  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(i / 2, test_receiver_video_callback_->number_times_called());
}

TEST_F(End2EndTest, CryptoVideo) {
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_PCM16, 32000, 1);

  video_sender_config_.aes_iv_mask =
      ConvertFromBase16String("1234567890abcdeffedcba0987654321");
  video_sender_config_.aes_key =
      ConvertFromBase16String("deadbeefcafeb0b0b0b0cafedeadbeef");

  video_receiver_config_.aes_iv_mask =
      video_sender_config_.aes_iv_mask;
  video_receiver_config_.aes_key =
      video_sender_config_.aes_key;

  Create();

  int frames_counter = 0;
  for (; frames_counter < 3; ++frames_counter) {
    const base::TimeTicks capture_time = testing_clock_sender_->NowTicks();
    SendVideoFrame(frames_counter, capture_time);

    test_receiver_video_callback_->AddExpectedResult(
        frames_counter,
        video_sender_config_.width,
        video_sender_config_.height,
        capture_time + base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
        true);

    RunTasks(kFrameTimerMs);

    cast_receiver_->RequestDecodedVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_));
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(frames_counter,
            test_receiver_video_callback_->number_times_called());
}

TEST_F(End2EndTest, CryptoAudio) {
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_PCM16, 32000, 1);

  audio_sender_config_.aes_iv_mask =
      ConvertFromBase16String("abcdeffedcba12345678900987654321");
  audio_sender_config_.aes_key =
      ConvertFromBase16String("deadbeefcafecafedeadbeefb0b0b0b0");

  audio_receiver_config_.aes_iv_mask =
      audio_sender_config_.aes_iv_mask;
  audio_receiver_config_.aes_key =
      audio_sender_config_.aes_key;

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
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_PCM16, 32000, 1);
  Create();

  int video_start = kVideoStart;
  const int num_frames = 5;
  for (int i = 0; i < num_frames; ++i) {
    base::TimeTicks capture_time = testing_clock_sender_->NowTicks();
    test_receiver_video_callback_->AddExpectedResult(
        video_start,
        video_sender_config_.width,
        video_sender_config_.height,
        capture_time + base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
        true);

    SendVideoFrame(video_start, capture_time);
    RunTasks(kFrameTimerMs);

    cast_receiver_->RequestDecodedVideoFrame(
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
    for (int i = 0; i <= kNumOfLoggingEvents; ++i) {
      total_event_count_for_frame += map_it->second.counter[i];
    }

    int expected_event_count_for_frame = 0;

    EXPECT_EQ(1, map_it->second.counter[FRAME_CAPTURE_BEGIN]);
    expected_event_count_for_frame +=
        map_it->second.counter[FRAME_CAPTURE_BEGIN];

    EXPECT_EQ(1, map_it->second.counter[FRAME_CAPTURE_END]);
    expected_event_count_for_frame +=
        map_it->second.counter[FRAME_CAPTURE_END];

    EXPECT_EQ(1, map_it->second.counter[FRAME_ENCODED]);
    expected_event_count_for_frame +=
        map_it->second.counter[FRAME_ENCODED];

    EXPECT_EQ(1, map_it->second.counter[FRAME_DECODED]);
    expected_event_count_for_frame +=
        map_it->second.counter[FRAME_DECODED];

    EXPECT_EQ(1, map_it->second.counter[FRAME_PLAYOUT]);
    expected_event_count_for_frame += map_it->second.counter[FRAME_PLAYOUT];


    // There is no guarantee that FRAME_ACK_SENT is loggeed exactly once per
    // frame.
    EXPECT_GT(map_it->second.counter[FRAME_ACK_SENT], 0);
    expected_event_count_for_frame += map_it->second.counter[FRAME_ACK_SENT];

    // There is no guarantee that FRAME_ACK_RECEIVED is loggeed exactly once per
    // frame.
    EXPECT_GT(map_it->second.counter[FRAME_ACK_RECEIVED], 0);
    expected_event_count_for_frame +=
        map_it->second.counter[FRAME_ACK_RECEIVED];

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
    for (int i = 0; i <= kNumOfLoggingEvents; ++i) {
      total_event_count_for_packet += map_it->second.counter[i];
    }

    EXPECT_GT(map_it->second.counter[PACKET_RECEIVED], 0);
    int packets_received = map_it->second.counter[PACKET_RECEIVED];
    int packets_sent = map_it->second.counter[PACKET_SENT_TO_NETWORK];
    EXPECT_EQ(packets_sent, packets_received);

    // Verify that there were no other events logged with respect to this
    // packet. (i.e. Total event count = packets sent + packets received)
    EXPECT_EQ(packets_received + packets_sent, total_event_count_for_packet);
  }
}

// Audio test without packet loss - tests the logging aspects of the end2end,
// but is basically equivalent to LoopNoLossPcm16.
TEST_F(End2EndTest, AudioLogging) {
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_PCM16, 32000, 1);
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

  int encoded_count = 0;

  // Verify the right number of events were logged for each event type.
  for (std::map<RtpTimestamp, LoggingEventCounts>::iterator it =
           event_counter_for_frame.begin();
       it != event_counter_for_frame.end();
       ++it) {
    encoded_count += it->second.counter[FRAME_ENCODED];
  }

  EXPECT_EQ(num_audio_frames_requested, encoded_count);

  // Verify that each frame have the expected types of events logged.
  for (std::map<RtpTimestamp, LoggingEventCounts>::const_iterator map_it =
           event_counter_for_frame.begin();
       map_it != event_counter_for_frame.end(); ++map_it) {
    int total_event_count_for_frame = 0;
    for (int j = 0; j <= kNumOfLoggingEvents; ++j)
      total_event_count_for_frame += map_it->second.counter[j];

    int expected_event_count_for_frame = 0;

    EXPECT_EQ(1, map_it->second.counter[FRAME_ENCODED]);
    expected_event_count_for_frame +=
        map_it->second.counter[FRAME_ENCODED];

    EXPECT_EQ(1, map_it->second.counter[FRAME_PLAYOUT]);
    expected_event_count_for_frame +=
        map_it->second.counter[FRAME_PLAYOUT];

    EXPECT_EQ(1, map_it->second.counter[FRAME_DECODED]);
    expected_event_count_for_frame +=
        map_it->second.counter[FRAME_DECODED];

    EXPECT_GT(map_it->second.counter[FRAME_ACK_SENT], 0);
    EXPECT_GT(map_it->second.counter[FRAME_ACK_RECEIVED], 0);
    expected_event_count_for_frame += map_it->second.counter[FRAME_ACK_SENT];
    expected_event_count_for_frame +=
        map_it->second.counter[FRAME_ACK_RECEIVED];

    // Verify that there were no other events logged with respect to this frame.
    // (i.e. Total event count = expected event count)
    EXPECT_EQ(total_event_count_for_frame, expected_event_count_for_frame);
  }
}

TEST_F(End2EndTest, BasicFakeSoftwareVideo) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16, 32000,
            1);
  Create();
  StartBasicPlayer();
  SetReceiverSkew(1.0, base::TimeDelta::FromMilliseconds(1));

  // Expect very smooth playout when there is no clock skew.
  SetExpectedVideoPlayoutSmoothness(
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 99 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 101 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) / 100);

  int frames_counter = 0;
  for (; frames_counter < 1000; ++frames_counter) {
    SendFakeVideoFrame(testing_clock_sender_->NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(1000ul, video_ticks_.size());
}

TEST_F(End2EndTest, ReceiverClockFast) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16, 32000,
            1);
  Create();
  StartBasicPlayer();
  SetReceiverSkew(2.0, base::TimeDelta::FromMicroseconds(1234567));

  int frames_counter = 0;
  for (; frames_counter < 10000; ++frames_counter) {
    SendFakeVideoFrame(testing_clock_sender_->NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(10000ul, video_ticks_.size());
}

TEST_F(End2EndTest, ReceiverClockSlow) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16, 32000,
            1);
  Create();
  StartBasicPlayer();
  SetReceiverSkew(0.5, base::TimeDelta::FromMicroseconds(-765432));

  int frames_counter = 0;
  for (; frames_counter < 10000; ++frames_counter) {
    SendFakeVideoFrame(testing_clock_sender_->NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(10000ul, video_ticks_.size());
}

TEST_F(End2EndTest, SmoothPlayoutWithFivePercentClockRateSkew) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16, 32000,
            1);
  Create();
  StartBasicPlayer();
  SetReceiverSkew(1.05, base::TimeDelta::FromMilliseconds(-42));

  // Expect smooth playout when there is 5% skew.
  SetExpectedVideoPlayoutSmoothness(
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 90 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 110 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) / 10);

  int frames_counter = 0;
  for (; frames_counter < 10000; ++frames_counter) {
    SendFakeVideoFrame(testing_clock_sender_->NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(10000ul, video_ticks_.size());
}

TEST_F(End2EndTest, EvilNetwork) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16, 32000,
            1);
  receiver_to_sender_.SetPacketPipe(test::EvilNetwork().Pass());
  sender_to_receiver_.SetPacketPipe(test::EvilNetwork().Pass());
  Create();
  StartBasicPlayer();

  int frames_counter = 0;
  for (; frames_counter < 10000; ++frames_counter) {
    SendFakeVideoFrame(testing_clock_sender_->NowTicks());
    RunTasks(kFrameTimerMs);
  }
  base::TimeTicks test_end = testing_clock_receiver_->NowTicks();
  RunTasks(100 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_GT(video_ticks_.size(), 100ul);
  EXPECT_LT((video_ticks_.back().second - test_end).InMilliseconds(), 1000);
}

TEST_F(End2EndTest, OldPacketNetwork) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16, 32000, 1);
  sender_to_receiver_.SetPacketPipe(test::NewRandomDrop(0.01));
  scoped_ptr<test::PacketPipe> echo_chamber(
      test::NewDuplicateAndDelay(1, 10 * kFrameTimerMs));
  echo_chamber->AppendToPipe(
      test::NewDuplicateAndDelay(1, 20 * kFrameTimerMs));
  echo_chamber->AppendToPipe(
      test::NewDuplicateAndDelay(1, 40 * kFrameTimerMs));
  echo_chamber->AppendToPipe(
      test::NewDuplicateAndDelay(1, 80 * kFrameTimerMs));
  echo_chamber->AppendToPipe(
      test::NewDuplicateAndDelay(1, 160 * kFrameTimerMs));

  receiver_to_sender_.SetPacketPipe(echo_chamber.Pass());
  Create();
  StartBasicPlayer();

  SetExpectedVideoPlayoutSmoothness(
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 90 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 110 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) / 10);

  int frames_counter = 0;
  for (; frames_counter < 10000; ++frames_counter) {
    SendFakeVideoFrame(testing_clock_sender_->NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(100 * kFrameTimerMs + 1);  // Empty the pipeline.

  EXPECT_EQ(10000ul, video_ticks_.size());
}

TEST_F(End2EndTest, TestSetPlayoutDelay) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16, 32000, 1);
  Create();
  StartBasicPlayer();
  const int kNewDelay = 600;

  int frames_counter = 0;
  for (; frames_counter < 200; ++frames_counter) {
    SendFakeVideoFrame(testing_clock_sender_->NowTicks());
    RunTasks(kFrameTimerMs);
  }
  cast_sender_->SetTargetPlayoutDelay(
      base::TimeDelta::FromMilliseconds(kNewDelay));
  for (; frames_counter < 400; ++frames_counter) {
    SendFakeVideoFrame(testing_clock_sender_->NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(100 * kFrameTimerMs + 1);  // Empty the pipeline.
  size_t jump = 0;
  for (size_t i = 1; i < video_ticks_.size(); i++) {
    int64 delta = (video_ticks_[i].second -
                   video_ticks_[i-1].second).InMilliseconds();
    if (delta > 100) {
      EXPECT_EQ(delta, kNewDelay - kTargetPlayoutDelayMs + kFrameTimerMs);
      EXPECT_EQ(0u, jump);
      jump = i;
    }
  }
  EXPECT_GT(jump, 199u);
  EXPECT_LT(jump, 220u);
}

// TODO(pwestin): Add repeatable packet loss test.
// TODO(pwestin): Add test for misaligned send get calls.
// TODO(pwestin): Add more tests that does not resample.
// TODO(pwestin): Add test when we have starvation for our RunTask.

}  // namespace cast
}  // namespace media
