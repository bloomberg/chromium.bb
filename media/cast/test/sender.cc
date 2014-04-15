// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test application that simulates a cast sender - Data can be either generated
// or read from a file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_file.h"
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
#include "media/cast/test/utility/default_config.h"
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

#define DEFAULT_LOGGING_DURATION "10"
#define DEFAULT_COMPRESS_LOGS "1"

namespace {
static const int kAudioChannels = 2;
static const int kAudioSamplingFrequency = 48000;
static const int kSoundFrequency = 1234;  // Frequency of sinusoid wave.
// The tests are commonly implemented with |kFrameTimerMs| RunTask function;
// a normal video is 30 fps hence the 33 ms between frames.
static const float kSoundVolume = 0.5f;
static const int kFrameTimerMs = 33;

// The max allowed size of serialized log.
const int kMaxSerializedLogBytes = 10 * 1000 * 1000;
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

std::string GetVideoLogFileDestination(bool compress) {
  test::InputBuilder input(
      "Enter video events log file destination.",
      compress ? "./video_events.log.gz" : "./video_events.log",
      INT_MIN,
      INT_MAX);
  return input.GetStringInput();
}

std::string GetAudioLogFileDestination(bool compress) {
  test::InputBuilder input(
      "Enter audio events log file destination.",
      compress ? "./audio_events.log.gz" : "./audio_events.log",
      INT_MIN,
      INT_MAX);
  return input.GetStringInput();
}

bool CompressLogs() {
  test::InputBuilder input(
      "Enter 1 to enable compression on logs.", DEFAULT_COMPRESS_LOGS, 0, 1);
  return (1 == input.GetIntInput());
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
  return video_config;
}

class SendProcess {
 public:
  SendProcess(scoped_refptr<base::SingleThreadTaskRunner> thread_proxy,
              base::TickClock* clock,
              const VideoSenderConfig& video_config,
              scoped_refptr<AudioFrameInput> audio_frame_input,
              scoped_refptr<VideoFrameInput> video_frame_input)
      : test_app_thread_proxy_(thread_proxy),
        video_config_(video_config),
        audio_diff_(kFrameTimerMs),
        audio_frame_input_(audio_frame_input),
        video_frame_input_(video_frame_input),
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

    audio_frame_input_->InsertAudio(
        audio_bus_factory_->NextAudioBus(
            base::TimeDelta::FromMilliseconds(10) * num_10ms_blocks),
        clock_->NowTicks());

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
                     weak_factory_.GetWeakPtr(),
                     video_frame),
          video_frame_time - elapsed_time);
    } else {
      test_app_thread_proxy_->PostTask(
          FROM_HERE,
          base::Bind(&SendProcess::SendVideoFrameOnTime,
                     weak_factory_.GetWeakPtr(),
                     video_frame));
    }
  }

  void SendVideoFrameOnTime(scoped_refptr<media::VideoFrame> video_frame) {
    send_time_ = clock_->NowTicks();
    video_frame_input_->InsertRawVideoFrame(video_frame, send_time_);
    test_app_thread_proxy_->PostTask(
        FROM_HERE, base::Bind(&SendProcess::SendFrame, base::Unretained(this)));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> test_app_thread_proxy_;
  const VideoSenderConfig video_config_;
  int audio_diff_;
  const scoped_refptr<AudioFrameInput> audio_frame_input_;
  const scoped_refptr<VideoFrameInput> video_frame_input_;
  FILE* video_file_;
  uint8 synthetic_count_;
  base::TickClock* const clock_;  // Not owned by this class.
  base::TimeTicks start_time_;
  base::TimeTicks send_time_;
  scoped_ptr<TestAudioBusFactory> audio_bus_factory_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<SendProcess> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SendProcess);
};

}  // namespace cast
}  // namespace media

namespace {
void UpdateCastTransportStatus(
    media::cast::transport::CastTransportStatus status) {}

void LogRawEvents(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    const std::vector<media::cast::PacketEvent>& packet_events) {
  VLOG(1) << "Got packet events from transport, size: " << packet_events.size();
  for (std::vector<media::cast::PacketEvent>::const_iterator it =
           packet_events.begin();
       it != packet_events.end();
       ++it) {
    cast_environment->Logging()->InsertPacketEvent(it->timestamp,
                                                   it->type,
                                                   it->rtp_timestamp,
                                                   it->frame_id,
                                                   it->packet_id,
                                                   it->max_packet_id,
                                                   it->size);
  }
}

void InitializationResult(media::cast::CastInitializationStatus result) {
  bool end_result = result == media::cast::STATUS_AUDIO_INITIALIZED ||
                    result == media::cast::STATUS_VIDEO_INITIALIZED;
  CHECK(end_result) << "Cast sender uninitialized";
}

net::IPEndPoint CreateUDPAddress(std::string ip_str, int port) {
  net::IPAddressNumber ip_number;
  CHECK(net::ParseIPLiteralToNumber(ip_str, &ip_number));
  return net::IPEndPoint(ip_number, port);
}

void DumpLoggingData(const media::cast::proto::LogMetadata& log_metadata,
                     const media::cast::FrameEventMap& frame_events,
                     const media::cast::PacketEventMap& packet_events,
                     bool compress,
                     base::ScopedFILE log_file) {
  VLOG(0) << "Frame map size: " << frame_events.size();
  VLOG(0) << "Packet map size: " << packet_events.size();

  scoped_ptr<char[]> event_log(new char[media::cast::kMaxSerializedLogBytes]);
  int event_log_bytes;
  if (!media::cast::SerializeEvents(log_metadata,
                                    frame_events,
                                    packet_events,
                                    compress,
                                    media::cast::kMaxSerializedLogBytes,
                                    event_log.get(),
                                    &event_log_bytes)) {
    VLOG(0) << "Failed to serialize events.";
    return;
  }

  VLOG(0) << "Events serialized length: " << event_log_bytes;

  int ret = fwrite(event_log.get(), 1, event_log_bytes, log_file.get());
  if (ret != event_log_bytes)
    VLOG(0) << "Failed to write logs to file.";
}

void WriteLogsToFileAndStopSubscribing(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    scoped_ptr<media::cast::EncodingEventSubscriber> video_event_subscriber,
    scoped_ptr<media::cast::EncodingEventSubscriber> audio_event_subscriber,
    base::ScopedFILE video_log_file,
    base::ScopedFILE audio_log_file,
    bool compress) {
  cast_environment->Logging()->RemoveRawEventSubscriber(
      video_event_subscriber.get());
  cast_environment->Logging()->RemoveRawEventSubscriber(
      audio_event_subscriber.get());

  VLOG(0) << "Dumping logging data for video stream.";
  media::cast::proto::LogMetadata log_metadata;
  media::cast::FrameEventMap frame_events;
  media::cast::PacketEventMap packet_events;
  video_event_subscriber->GetEventsAndReset(
      &log_metadata, &frame_events, &packet_events);

  DumpLoggingData(log_metadata,
                  frame_events,
                  packet_events,
                  compress,
                  video_log_file.Pass());

  VLOG(0) << "Dumping logging data for audio stream.";
  audio_event_subscriber->GetEventsAndReset(
      &log_metadata, &frame_events, &packet_events);

  DumpLoggingData(log_metadata,
                  frame_events,
                  packet_events,
                  compress,
                  audio_log_file.Pass());
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());
  base::Thread test_thread("Cast sender test app thread");
  base::Thread audio_thread("Cast audio encoder thread");
  base::Thread video_thread("Cast video encoder thread");
  test_thread.Start();
  audio_thread.Start();
  video_thread.Start();

  base::MessageLoopForIO io_message_loop;

  int remote_port;
  media::cast::GetPort(&remote_port);

  std::string remote_ip_address =
      media::cast::GetIpAddress("Enter receiver IP.");

  media::cast::AudioSenderConfig audio_config =
      media::cast::GetAudioSenderConfig();
  media::cast::VideoSenderConfig video_config =
      media::cast::GetVideoSenderConfig();

  // Running transport on the main thread.
  // Setting up transport config.
  media::cast::transport::CastTransportAudioConfig transport_audio_config;
  media::cast::transport::CastTransportVideoConfig transport_video_config;
  net::IPEndPoint remote_endpoint =
      CreateUDPAddress(remote_ip_address, remote_port);
  transport_audio_config.base.ssrc = audio_config.sender_ssrc;
  VLOG(0) << "Audio ssrc: " << transport_audio_config.base.ssrc;
  transport_audio_config.base.rtp_config = audio_config.rtp_config;
  transport_video_config.base.ssrc = video_config.sender_ssrc;
  transport_video_config.base.rtp_config = video_config.rtp_config;

  // Enable raw event and stats logging.
  // Running transport on the main thread.
  scoped_refptr<media::cast::CastEnvironment> cast_environment(
      new media::cast::CastEnvironment(
          make_scoped_ptr<base::TickClock>(new base::DefaultTickClock()),
          io_message_loop.message_loop_proxy(),
          audio_thread.message_loop_proxy(),
          video_thread.message_loop_proxy()));

  scoped_ptr<media::cast::transport::CastTransportSender> transport_sender =
      media::cast::transport::CastTransportSender::Create(
          NULL,  // net log.
          cast_environment->Clock(),
          remote_endpoint,
          base::Bind(&UpdateCastTransportStatus),
          base::Bind(&LogRawEvents, cast_environment),
          base::TimeDelta::FromSeconds(1),
          io_message_loop.message_loop_proxy());

  transport_sender->InitializeAudio(transport_audio_config);
  transport_sender->InitializeVideo(transport_video_config);

  scoped_ptr<media::cast::CastSender> cast_sender =
      media::cast::CastSender::Create(cast_environment, transport_sender.get());

  cast_sender->InitializeVideo(
      video_config,
      base::Bind(&InitializationResult),
      media::cast::CreateDefaultVideoEncodeAcceleratorCallback(),
      media::cast::CreateDefaultVideoEncodeMemoryCallback());

  cast_sender->InitializeAudio(audio_config, base::Bind(&InitializationResult));

  transport_sender->SetPacketReceiver(cast_sender->packet_receiver());

  scoped_refptr<media::cast::AudioFrameInput> audio_frame_input =
      cast_sender->audio_frame_input();
  scoped_refptr<media::cast::VideoFrameInput> video_frame_input =
      cast_sender->video_frame_input();
  scoped_ptr<media::cast::SendProcess> send_process(
      new media::cast::SendProcess(test_thread.message_loop_proxy(),
                                   cast_environment->Clock(),
                                   video_config,
                                   audio_frame_input,
                                   video_frame_input));

  // Set up event subscribers.
  int logging_duration = media::cast::GetLoggingDuration();
  scoped_ptr<media::cast::EncodingEventSubscriber> video_event_subscriber;
  scoped_ptr<media::cast::EncodingEventSubscriber> audio_event_subscriber;
  if (logging_duration > 0) {
    bool compress = media::cast::CompressLogs();
    std::string video_log_file_name(
        media::cast::GetVideoLogFileDestination(compress));
    std::string audio_log_file_name(
        media::cast::GetAudioLogFileDestination(compress));
    video_event_subscriber.reset(new media::cast::EncodingEventSubscriber(
        media::cast::VIDEO_EVENT, 10000));
    audio_event_subscriber.reset(new media::cast::EncodingEventSubscriber(
        media::cast::AUDIO_EVENT, 10000));
    cast_environment->Logging()->AddRawEventSubscriber(
        video_event_subscriber.get());
    cast_environment->Logging()->AddRawEventSubscriber(
        audio_event_subscriber.get());

    base::ScopedFILE video_log_file(fopen(video_log_file_name.c_str(), "w"));
    if (!video_log_file) {
      VLOG(1) << "Failed to open video log file for writing.";
      exit(-1);
    }

    base::ScopedFILE audio_log_file(fopen(audio_log_file_name.c_str(), "w"));
    if (!audio_log_file) {
      VLOG(1) << "Failed to open audio log file for writing.";
      exit(-1);
    }

    io_message_loop.message_loop_proxy()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&WriteLogsToFileAndStopSubscribing,
                   cast_environment,
                   base::Passed(&video_event_subscriber),
                   base::Passed(&audio_event_subscriber),
                   base::Passed(&video_log_file),
                   base::Passed(&audio_log_file),
                   compress),
        base::TimeDelta::FromSeconds(logging_duration));
  }

  test_thread.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&media::cast::SendProcess::SendFrame,
                 base::Unretained(send_process.get())));

  io_message_loop.Run();

  return 0;
}
