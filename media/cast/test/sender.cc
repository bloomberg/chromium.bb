// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test application that simulates a cast sender - Data can be either generated
// or read from a file.

#include "base/at_exit.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/encoding_event_subscriber.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/input_builder.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/cast_transport_sender.h"
#include "media/cast/transport/transport/udp_transport.h"
#include "ui/gfx/size.h"

namespace media {
namespace cast {
// Settings chosen to match default receiver settings.
#define DEFAULT_RECEIVER_PORT "2344"
#define DEFAULT_RECEIVER_IP "127.0.0.1"
#define DEFAULT_READ_FROM_FILE "0"
#define DEFAULT_AUDIO_SENDER_SSRC "1"
#define DEFAULT_AUDIO_RECEIVER_SSRC "2"
#define DEFAULT_AUDIO_PAYLOAD_TYPE "127"
#define DEFAULT_VIDEO_SENDER_SSRC "11"
#define DEFAULT_VIDEO_RECEIVER_SSRC "12"
#define DEFAULT_VIDEO_PAYLOAD_TYPE "96"
#define DEFAULT_VIDEO_CODEC_WIDTH "1280"
#define DEFAULT_VIDEO_CODEC_HEIGHT "720"
#define DEFAULT_VIDEO_CODEC_BITRATE "2000"
#define DEFAULT_VIDEO_CODEC_MAX_BITRATE "4000"
#define DEFAULT_VIDEO_CODEC_MIN_BITRATE "1000"

#define DEFAULT_LOGGING_DURATION "30"

namespace {
static const int kAudioChannels = 2;
static const int kAudioSamplingFrequency = 48000;
static const int kSoundFrequency = 1234;  // Frequency of sinusoid wave.
// The tests are commonly implemented with |kFrameTimerMs| RunTask function;
// a normal video is 30 fps hence the 33 ms between frames.
static const float kSoundVolume = 0.5f;
static const int kFrameTimerMs = 33;

// Dummy callback function that does nothing except to accept ownership of
// |audio_bus| for destruction. This guarantees that the audio_bus is valid for
// the entire duration of the encode/send process (not equivalent to DoNothing).
void OwnThatAudioBus(scoped_ptr<AudioBus> audio_bus) {}
}  // namespace

void GetPort(int* port) {
  test::InputBuilder input(
      "Enter receiver port.", DEFAULT_RECEIVER_PORT, 1, INT_MAX);
  *port = input.GetIntInput();
}

std::string GetIpAddress(const std::string display_text) {
  test::InputBuilder input(display_text, DEFAULT_RECEIVER_IP, INT_MIN, INT_MAX);
  std::string ip_address = input.GetStringInput();
  // Verify correct form:
  while (std::count(ip_address.begin(), ip_address.end(), '.') != 3) {
    ip_address = input.GetStringInput();
  }
  return ip_address;
}

int GetLoggingDuration() {
  test::InputBuilder input(
      "Choose logging duration (seconds), 0 for no logging.",
      DEFAULT_LOGGING_DURATION,
      0,
      INT_MAX);
  return input.GetIntInput();
}

std::string GetLogFileDestination() {
  test::InputBuilder input(
      "Enter log file destination.", "./raw_events.log", INT_MIN, INT_MAX);
  return input.GetStringInput();
}

bool ReadFromFile() {
  test::InputBuilder input(
      "Enter 1 to read from file.", DEFAULT_READ_FROM_FILE, 0, 1);
  return (1 == input.GetIntInput());
}

std::string GetVideoFile() {
  test::InputBuilder input(
      "Enter file and path to raw video file.", "", INT_MIN, INT_MAX);
  return input.GetStringInput();
}

void GetSsrcs(AudioSenderConfig* audio_config) {
  test::InputBuilder input_tx(
      "Choose audio sender SSRC.", DEFAULT_AUDIO_SENDER_SSRC, 1, INT_MAX);
  audio_config->sender_ssrc = input_tx.GetIntInput();

  test::InputBuilder input_rx(
      "Choose audio receiver SSRC.", DEFAULT_AUDIO_RECEIVER_SSRC, 1, INT_MAX);
  audio_config->incoming_feedback_ssrc = input_rx.GetIntInput();
}

void GetSsrcs(VideoSenderConfig* video_config) {
  test::InputBuilder input_tx(
      "Choose video sender SSRC.", DEFAULT_VIDEO_SENDER_SSRC, 1, INT_MAX);
  video_config->sender_ssrc = input_tx.GetIntInput();

  test::InputBuilder input_rx(
      "Choose video receiver SSRC.", DEFAULT_VIDEO_RECEIVER_SSRC, 1, INT_MAX);
  video_config->incoming_feedback_ssrc = input_rx.GetIntInput();
}

void GetPayloadtype(AudioSenderConfig* audio_config) {
  test::InputBuilder input(
      "Choose audio sender payload type.", DEFAULT_AUDIO_PAYLOAD_TYPE, 96, 127);
  audio_config->rtp_config.payload_type = input.GetIntInput();
}

AudioSenderConfig GetAudioSenderConfig() {
  AudioSenderConfig audio_config;

  GetSsrcs(&audio_config);
  GetPayloadtype(&audio_config);

  audio_config.rtcp_c_name = "audio_sender@a.b.c.d";

  VLOG(0) << "Using OPUS 48Khz stereo at 64kbit/s";
  audio_config.use_external_encoder = false;
  audio_config.frequency = kAudioSamplingFrequency;
  audio_config.channels = kAudioChannels;
  audio_config.bitrate = 64000;
  audio_config.codec = transport::kOpus;
  return audio_config;
}

void GetPayloadtype(VideoSenderConfig* video_config) {
  test::InputBuilder input(
      "Choose video sender payload type.", DEFAULT_VIDEO_PAYLOAD_TYPE, 96, 127);
  video_config->rtp_config.payload_type = input.GetIntInput();
}

void GetVideoCodecSize(VideoSenderConfig* video_config) {
  test::InputBuilder input_width(
      "Choose video width.", DEFAULT_VIDEO_CODEC_WIDTH, 144, 1920);
  video_config->width = input_width.GetIntInput();

  test::InputBuilder input_height(
      "Choose video height.", DEFAULT_VIDEO_CODEC_HEIGHT, 176, 1080);
  video_config->height = input_height.GetIntInput();
}

void GetVideoBitrates(VideoSenderConfig* video_config) {
  test::InputBuilder input_start_br(
      "Choose start bitrate[kbps].", DEFAULT_VIDEO_CODEC_BITRATE, 0, INT_MAX);
  video_config->start_bitrate = input_start_br.GetIntInput() * 1000;

  test::InputBuilder input_max_br(
      "Choose max bitrate[kbps].", DEFAULT_VIDEO_CODEC_MAX_BITRATE, 0, INT_MAX);
  video_config->max_bitrate = input_max_br.GetIntInput() * 1000;

  test::InputBuilder input_min_br(
      "Choose min bitrate[kbps].", DEFAULT_VIDEO_CODEC_MIN_BITRATE, 0, INT_MAX);
  video_config->min_bitrate = input_min_br.GetIntInput() * 1000;
}

VideoSenderConfig GetVideoSenderConfig() {
  VideoSenderConfig video_config;

  GetSsrcs(&video_config);
  GetPayloadtype(&video_config);
  GetVideoCodecSize(&video_config);
  GetVideoBitrates(&video_config);

  video_config.rtcp_c_name = "video_sender@a.b.c.d";

  video_config.use_external_encoder = false;

  VLOG(0) << "Using VP8 at 30 fps";
  video_config.min_qp = 4;
  video_config.max_qp = 40;
  video_config.max_frame_rate = 30;
  video_config.codec = transport::kVp8;
  video_config.max_number_of_video_buffers_used = 1;
  video_config.number_of_cores = 1;
  return video_config;
}

class SendProcess {
 public:
  SendProcess(scoped_refptr<base::SingleThreadTaskRunner> thread_proxy,
              base::TickClock* clock,
              const VideoSenderConfig& video_config,
              FrameInput* frame_input)
      : test_app_thread_proxy_(thread_proxy),
        video_config_(video_config),
        audio_diff_(kFrameTimerMs),
        frame_input_(frame_input),
        synthetic_count_(0),
        clock_(clock),
        start_time_(),
        send_time_(),
        weak_factory_(this) {
    audio_bus_factory_.reset(new TestAudioBusFactory(kAudioChannels,
                                                     kAudioSamplingFrequency,
                                                     kSoundFrequency,
                                                     kSoundVolume));
    if (ReadFromFile()) {
      std::string video_file_name = GetVideoFile();
      video_file_ = fopen(video_file_name.c_str(), "r");
      if (video_file_ == NULL) {
        VLOG(1) << "Failed to open file";
        exit(-1);
      }
    } else {
      video_file_ = NULL;
    }
  }

  ~SendProcess() {
    if (video_file_)
      fclose(video_file_);
  }

  void SendFrame() {
    // Make sure that we don't drift.
    int num_10ms_blocks = audio_diff_ / 10;
    // Avoid drift.
    audio_diff_ += kFrameTimerMs - num_10ms_blocks * 10;

    scoped_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
        base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks));
    AudioBus* const audio_bus_ptr = audio_bus.get();
    frame_input_->InsertAudio(
        audio_bus_ptr,
        clock_->NowTicks(),
        base::Bind(&OwnThatAudioBus, base::Passed(&audio_bus)));

    gfx::Size size(video_config_.width, video_config_.height);
    // TODO(mikhal): Use the provided timestamp.
    if (start_time_.is_null())
      start_time_ = clock_->NowTicks();
    base::TimeDelta time_diff = clock_->NowTicks() - start_time_;
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
            VideoFrame::I420, size, gfx::Rect(size), size, time_diff);
    if (video_file_) {
      if (!PopulateVideoFrameFromFile(video_frame, video_file_))
        return;
    } else {
      PopulateVideoFrame(video_frame, synthetic_count_);
      ++synthetic_count_;
    }

    // Time the sending of the frame to match the set frame rate.
    // Sleep if that time has yet to elapse.
    base::TimeTicks now = clock_->NowTicks();
    base::TimeDelta video_frame_time =
        base::TimeDelta::FromMilliseconds(kFrameTimerMs);
    base::TimeDelta elapsed_time = now - send_time_;
    if (elapsed_time < video_frame_time) {
      VLOG(1) << "Wait" << (video_frame_time - elapsed_time).InMilliseconds();
      test_app_thread_proxy_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&SendProcess::SendVideoFrameOnTime,
                     base::Unretained(this),
                     video_frame),
          video_frame_time - elapsed_time);
    } else {
      test_app_thread_proxy_->PostTask(
          FROM_HERE,
          base::Bind(&SendProcess::SendVideoFrameOnTime,
                     base::Unretained(this),
                     video_frame));
    }
  }

  void SendVideoFrameOnTime(scoped_refptr<media::VideoFrame> video_frame) {
    send_time_ = clock_->NowTicks();
    frame_input_->InsertRawVideoFrame(video_frame, send_time_);
    test_app_thread_proxy_->PostTask(
        FROM_HERE, base::Bind(&SendProcess::SendFrame, base::Unretained(this)));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> test_app_thread_proxy_;
  const VideoSenderConfig video_config_;
  int audio_diff_;
  const scoped_refptr<FrameInput> frame_input_;
  FILE* video_file_;
  uint8 synthetic_count_;
  base::TickClock* const clock_;  // Not owned by this class.
  base::TimeTicks start_time_;
  base::TimeTicks send_time_;
  scoped_ptr<TestAudioBusFactory> audio_bus_factory_;
  base::WeakPtrFactory<SendProcess> weak_factory_;
};

}  // namespace cast
}  // namespace media

namespace {
void UpdateCastTransportStatus(
    media::cast::transport::CastTransportStatus status) {}

void InitializationResult(media::cast::CastInitializationStatus result) {
  CHECK_EQ(result, media::cast::STATUS_INITIALIZED);
  VLOG(1) << "Cast Sender initialized";
}

net::IPEndPoint CreateUDPAddress(std::string ip_str, int port) {
  net::IPAddressNumber ip_number;
  CHECK(net::ParseIPLiteralToNumber(ip_str, &ip_number));
  return net::IPEndPoint(ip_number, port);
}

void WriteLogsToFileAndStopSubscribing(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    scoped_ptr<media::cast::EncodingEventSubscriber> video_event_subscriber,
    scoped_ptr<media::cast::EncodingEventSubscriber> audio_event_subscriber,
    file_util::ScopedFILE log_file) {
  media::cast::LogSerializer serializer(media::cast::kMaxSerializedLogBytes);

  // Serialize video events.
  cast_environment->Logging()->RemoveRawEventSubscriber(
      video_event_subscriber.get());
  media::cast::FrameEventMap frame_events;
  media::cast::PacketEventMap packet_events;
  media::cast::RtpTimestamp first_rtp_timestamp;
  video_event_subscriber->GetEventsAndReset(&frame_events, &packet_events,
                                            &first_rtp_timestamp);

  VLOG(0) << "Video frame map size: " << frame_events.size();
  VLOG(0) << "Video packet map size: " << packet_events.size();

  if (!serializer.SerializeEventsForStream(false, frame_events, packet_events,
                                           first_rtp_timestamp)) {
    VLOG(1) << "Failed to serialize video events.";
    return;
  }

  int length_so_far = serializer.GetSerializedLength();
  VLOG(0) << "Video events serialized length: " << length_so_far;

  // Serialize audio events.
  cast_environment->Logging()->RemoveRawEventSubscriber(
      audio_event_subscriber.get());
  audio_event_subscriber->GetEventsAndReset(&frame_events, &packet_events,
                                            &first_rtp_timestamp);

  VLOG(0) << "Audio frame map size: " << frame_events.size();
  VLOG(0) << "Audio packet map size: " << packet_events.size();

  if (!serializer.SerializeEventsForStream(true, frame_events, packet_events,
                                           first_rtp_timestamp)) {
    VLOG(1) << "Failed to serialize audio events.";
    return;
  }

  VLOG(0) << "Audio events serialized length: "
          << serializer.GetSerializedLength() - length_so_far;

  scoped_ptr<std::string> serialized_string =
      serializer.GetSerializedLogAndReset();
  VLOG(0) << "Serialized string size: " << serialized_string->size();

  size_t ret = fwrite(
      &(*serialized_string)[0], 1, serialized_string->size(), log_file.get());
  if (ret != serialized_string->size())
    VLOG(1) << "Failed to write logs to file.";
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  VLOG(1) << "Cast Sender";
  base::Thread test_thread("Cast sender test app thread");
  base::Thread audio_thread("Cast audio encoder thread");
  base::Thread video_thread("Cast video encoder thread");
  test_thread.Start();
  audio_thread.Start();
  video_thread.Start();

  scoped_ptr<base::TickClock> clock(new base::DefaultTickClock());
  base::MessageLoopForIO io_message_loop;

  int remote_port;
  media::cast::GetPort(&remote_port);

  std::string remote_ip_address =
      media::cast::GetIpAddress("Enter receiver IP.");

  media::cast::AudioSenderConfig audio_config =
      media::cast::GetAudioSenderConfig();
  media::cast::VideoSenderConfig video_config =
      media::cast::GetVideoSenderConfig();

  // Setting up transport config.
  media::cast::transport::CastTransportConfig config;
  config.receiver_endpoint = CreateUDPAddress(remote_ip_address, remote_port);
  config.local_endpoint = CreateUDPAddress("0.0.0.0", 0);
  config.audio_ssrc = audio_config.sender_ssrc;
  config.video_ssrc = video_config.sender_ssrc;
  config.audio_rtp_config = audio_config.rtp_config;
  config.video_rtp_config = video_config.rtp_config;

  scoped_ptr<media::cast::transport::CastTransportSender> transport_sender(
      media::cast::transport::CastTransportSender::CreateCastTransportSender(
          NULL,
          clock.get(),
          config,
          base::Bind(&UpdateCastTransportStatus),
          io_message_loop.message_loop_proxy()));

  // Enable main and send side threads only. Enable raw event and stats logging.
  // Running transport on the main thread.
  scoped_refptr<media::cast::CastEnvironment> cast_environment(
      new media::cast::CastEnvironment(
          clock.Pass(),
          io_message_loop.message_loop_proxy(),
          audio_thread.message_loop_proxy(),
          NULL,
          video_thread.message_loop_proxy(),
          NULL,
          io_message_loop.message_loop_proxy(),
          media::cast::GetLoggingConfigWithRawEventsAndStatsEnabled()));

  scoped_ptr<media::cast::CastSender> cast_sender(
      media::cast::CastSender::CreateCastSender(
          cast_environment,
          &audio_config,
          &video_config,
          NULL,  // gpu_factories.
          base::Bind(&InitializationResult),
          transport_sender.get()));

  transport_sender->SetPacketReceiver(cast_sender->packet_receiver());

  media::cast::FrameInput* frame_input = cast_sender->frame_input();
  scoped_ptr<media::cast::SendProcess> send_process(
      new media::cast::SendProcess(test_thread.message_loop_proxy(),
                                   cast_environment->Clock(),
                                   video_config,
                                   frame_input));

  // Set up event subscribers.
  // TODO(imcheng): Set up separate subscribers for audio / video / other.
  int logging_duration = media::cast::GetLoggingDuration();
  scoped_ptr<media::cast::EncodingEventSubscriber> video_event_subscriber;
  scoped_ptr<media::cast::EncodingEventSubscriber> audio_event_subscriber;
  if (logging_duration > 0) {
    std::string log_file_name(media::cast::GetLogFileDestination());
    video_event_subscriber.reset(new media::cast::EncodingEventSubscriber(
        media::cast::VIDEO_EVENT, 10000));
    audio_event_subscriber.reset(new media::cast::EncodingEventSubscriber(
        media::cast::AUDIO_EVENT, 10000));
    cast_environment->Logging()->AddRawEventSubscriber(
        video_event_subscriber.get());
    cast_environment->Logging()->AddRawEventSubscriber(
        audio_event_subscriber.get());
    file_util::ScopedFILE log_file(fopen(log_file_name.c_str(), "w"));
    if (!log_file) {
      VLOG(1) << "Failed to open log file for writing.";
      exit(-1);
    }

    io_message_loop.message_loop_proxy()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&WriteLogsToFileAndStopSubscribing,
                   cast_environment,
                   base::Passed(&video_event_subscriber),
                   base::Passed(&audio_event_subscriber),
                   base::Passed(&log_file)),
        base::TimeDelta::FromSeconds(logging_duration));
  }

  test_thread.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&media::cast::SendProcess::SendFrame,
                 base::Unretained(send_process.get())));

  io_message_loop.Run();

  return 0;
}
