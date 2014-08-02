// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test application that simulates a cast sender - Data can be either generated
// or read from a file.

#include <queue>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "media/base/media.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/encoding_event_subscriber.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"
#include "media/cast/logging/receiver_time_offset_estimator_impl.h"
#include "media/cast/logging/stats_event_subscriber.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/cast_transport_sender.h"
#include "media/cast/net/udp_transport.h"
#include "media/cast/test/fake_media_source.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/input_builder.h"

namespace {
static const int kAudioChannels = 2;
static const int kAudioSamplingFrequency = 48000;

// The max allowed size of serialized log.
const int kMaxSerializedLogBytes = 10 * 1000 * 1000;

// Flags for this program:
//
// --address=xx.xx.xx.xx
//   IP address of receiver.
//
// --port=xxxx
//   Port number of receiver.
//
// --source-file=xxx.webm
//   WebM file as source of video frames.
//
// --fps=xx
//   Override framerate of the video stream.
const char kSwitchAddress[] = "address";
const char kSwitchPort[] = "port";
const char kSwitchSourceFile[] = "source-file";
const char kSwitchFps[] = "fps";

media::cast::AudioSenderConfig GetAudioSenderConfig() {
  media::cast::AudioSenderConfig audio_config;

  audio_config.use_external_encoder = false;
  audio_config.frequency = kAudioSamplingFrequency;
  audio_config.channels = kAudioChannels;
  audio_config.bitrate = 0;  // Use Opus auto-VBR mode.
  audio_config.codec = media::cast::CODEC_AUDIO_OPUS;
  audio_config.ssrc = 1;
  audio_config.incoming_feedback_ssrc = 2;
  audio_config.rtp_payload_type = 127;
  // TODO(miu): The default in cast_defines.h is 100.  Should this be 100, and
  // should receiver.cc's config also be 100?
  audio_config.target_playout_delay = base::TimeDelta::FromMilliseconds(300);
  return audio_config;
}

media::cast::VideoSenderConfig GetVideoSenderConfig() {
  media::cast::VideoSenderConfig video_config;

  video_config.use_external_encoder = false;

  // Resolution.
  video_config.width = 1280;
  video_config.height = 720;
  video_config.max_frame_rate = 30;

  // Bitrates.
  video_config.max_bitrate = 2500000;
  video_config.min_bitrate = 100000;
  video_config.start_bitrate = video_config.min_bitrate;

  // Codec.
  video_config.codec = media::cast::CODEC_VIDEO_VP8;
  video_config.max_number_of_video_buffers_used = 1;
  video_config.number_of_encode_threads = 2;

  // Quality options.
  video_config.min_qp = 4;
  video_config.max_qp = 40;

  // SSRCs and payload type. Don't change them.
  video_config.ssrc = 11;
  video_config.incoming_feedback_ssrc = 12;
  video_config.rtp_payload_type = 96;
  // TODO(miu): The default in cast_defines.h is 100.  Should this be 100, and
  // should receiver.cc's config also be 100?
  video_config.target_playout_delay = base::TimeDelta::FromMilliseconds(300);
  return video_config;
}

void UpdateCastTransportStatus(
    media::cast::CastTransportStatus status) {
  VLOG(1) << "Transport status: " << status;
}

void LogRawEvents(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    const std::vector<media::cast::PacketEvent>& packet_events,
    const std::vector<media::cast::FrameEvent>& frame_events) {
  VLOG(1) << "Got packet events from transport, size: " << packet_events.size();
  for (std::vector<media::cast::PacketEvent>::const_iterator it =
           packet_events.begin();
       it != packet_events.end();
       ++it) {
    cast_environment->Logging()->InsertPacketEvent(it->timestamp,
                                                   it->type,
                                                   it->media_type,
                                                   it->rtp_timestamp,
                                                   it->frame_id,
                                                   it->packet_id,
                                                   it->max_packet_id,
                                                   it->size);
  }
  VLOG(1) << "Got frame events from transport, size: " << frame_events.size();
  for (std::vector<media::cast::FrameEvent>::const_iterator it =
           frame_events.begin();
       it != frame_events.end();
       ++it) {
    cast_environment->Logging()->InsertFrameEvent(it->timestamp,
                                                  it->type,
                                                  it->media_type,
                                                  it->rtp_timestamp,
                                                  it->frame_id);
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
                     const media::cast::FrameEventList& frame_events,
                     const media::cast::PacketEventList& packet_events,
                     base::ScopedFILE log_file) {
  VLOG(0) << "Frame map size: " << frame_events.size();
  VLOG(0) << "Packet map size: " << packet_events.size();

  scoped_ptr<char[]> event_log(new char[kMaxSerializedLogBytes]);
  int event_log_bytes;
  if (!media::cast::SerializeEvents(log_metadata,
                                    frame_events,
                                    packet_events,
                                    true,
                                    kMaxSerializedLogBytes,
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

void WriteLogsToFileAndDestroySubscribers(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    scoped_ptr<media::cast::EncodingEventSubscriber> video_event_subscriber,
    scoped_ptr<media::cast::EncodingEventSubscriber> audio_event_subscriber,
    base::ScopedFILE video_log_file,
    base::ScopedFILE audio_log_file) {
  cast_environment->Logging()->RemoveRawEventSubscriber(
      video_event_subscriber.get());
  cast_environment->Logging()->RemoveRawEventSubscriber(
      audio_event_subscriber.get());

  VLOG(0) << "Dumping logging data for video stream.";
  media::cast::proto::LogMetadata log_metadata;
  media::cast::FrameEventList frame_events;
  media::cast::PacketEventList packet_events;
  video_event_subscriber->GetEventsAndReset(
      &log_metadata, &frame_events, &packet_events);

  DumpLoggingData(log_metadata,
                  frame_events,
                  packet_events,
                  video_log_file.Pass());

  VLOG(0) << "Dumping logging data for audio stream.";
  audio_event_subscriber->GetEventsAndReset(
      &log_metadata, &frame_events, &packet_events);

  DumpLoggingData(log_metadata,
                  frame_events,
                  packet_events,
                  audio_log_file.Pass());
}

void WriteStatsAndDestroySubscribers(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    scoped_ptr<media::cast::StatsEventSubscriber> video_event_subscriber,
    scoped_ptr<media::cast::StatsEventSubscriber> audio_event_subscriber,
    scoped_ptr<media::cast::ReceiverTimeOffsetEstimatorImpl> estimator) {
  cast_environment->Logging()->RemoveRawEventSubscriber(
      video_event_subscriber.get());
  cast_environment->Logging()->RemoveRawEventSubscriber(
      audio_event_subscriber.get());
  cast_environment->Logging()->RemoveRawEventSubscriber(estimator.get());

  scoped_ptr<base::DictionaryValue> stats = video_event_subscriber->GetStats();
  std::string json;
  base::JSONWriter::WriteWithOptions(
      stats.get(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  VLOG(0) << "Video stats: " << json;

  stats = audio_event_subscriber->GetStats();
  json.clear();
  base::JSONWriter::WriteWithOptions(
      stats.get(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  VLOG(0) << "Audio stats: " << json;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());

  // Load the media module for FFmpeg decoding.
  base::FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  if (!media::InitializeMediaLibrary(path)) {
    LOG(ERROR) << "Could not initialize media library.";
    return 1;
  }

  base::Thread test_thread("Cast sender test app thread");
  base::Thread audio_thread("Cast audio encoder thread");
  base::Thread video_thread("Cast video encoder thread");
  test_thread.Start();
  audio_thread.Start();
  video_thread.Start();

  base::MessageLoopForIO io_message_loop;

  // Default parameters.
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  std::string remote_ip_address = cmd->GetSwitchValueASCII(kSwitchAddress);
  if (remote_ip_address.empty())
    remote_ip_address = "127.0.0.1";
  int remote_port = 0;
  if (!base::StringToInt(cmd->GetSwitchValueASCII(kSwitchPort),
                         &remote_port)) {
    remote_port = 2344;
  }
  LOG(INFO) << "Sending to " << remote_ip_address << ":" << remote_port
            << ".";

  media::cast::AudioSenderConfig audio_config = GetAudioSenderConfig();
  media::cast::VideoSenderConfig video_config = GetVideoSenderConfig();

  // Running transport on the main thread.
  // Setting up transport config.
  net::IPEndPoint remote_endpoint =
      CreateUDPAddress(remote_ip_address, remote_port);

  // Enable raw event and stats logging.
  // Running transport on the main thread.
  scoped_refptr<media::cast::CastEnvironment> cast_environment(
      new media::cast::CastEnvironment(
          make_scoped_ptr<base::TickClock>(new base::DefaultTickClock()),
          io_message_loop.message_loop_proxy(),
          audio_thread.message_loop_proxy(),
          video_thread.message_loop_proxy()));

  // SendProcess initialization.
  scoped_ptr<media::cast::FakeMediaSource> fake_media_source(
      new media::cast::FakeMediaSource(test_thread.message_loop_proxy(),
                                       cast_environment->Clock(),
                                       video_config));

  int override_fps = 0;
  if (!base::StringToInt(cmd->GetSwitchValueASCII(kSwitchFps),
                         &override_fps)){
    override_fps = 0;
  }
  base::FilePath source_path = cmd->GetSwitchValuePath(kSwitchSourceFile);
  if (!source_path.empty()) {
    LOG(INFO) << "Source: " << source_path.value();
    fake_media_source->SetSourceFile(source_path, override_fps);
  }

  // CastTransportSender initialization.
  scoped_ptr<media::cast::CastTransportSender> transport_sender =
      media::cast::CastTransportSender::Create(
          NULL,  // net log.
          cast_environment->Clock(),
          remote_endpoint,
          base::Bind(&UpdateCastTransportStatus),
          base::Bind(&LogRawEvents, cast_environment),
          base::TimeDelta::FromSeconds(1),
          io_message_loop.message_loop_proxy());

  // CastSender initialization.
  scoped_ptr<media::cast::CastSender> cast_sender =
      media::cast::CastSender::Create(cast_environment, transport_sender.get());
  cast_sender->InitializeVideo(
      fake_media_source->get_video_config(),
      base::Bind(&InitializationResult),
      media::cast::CreateDefaultVideoEncodeAcceleratorCallback(),
      media::cast::CreateDefaultVideoEncodeMemoryCallback());
  cast_sender->InitializeAudio(audio_config, base::Bind(&InitializationResult));

  // Set up event subscribers.
  scoped_ptr<media::cast::EncodingEventSubscriber> video_event_subscriber;
  scoped_ptr<media::cast::EncodingEventSubscriber> audio_event_subscriber;
  std::string video_log_file_name("/tmp/video_events.log.gz");
  std::string audio_log_file_name("/tmp/audio_events.log.gz");
  LOG(INFO) << "Logging audio events to: " << audio_log_file_name;
  LOG(INFO) << "Logging video events to: " << video_log_file_name;
  video_event_subscriber.reset(new media::cast::EncodingEventSubscriber(
      media::cast::VIDEO_EVENT, 10000));
  audio_event_subscriber.reset(new media::cast::EncodingEventSubscriber(
      media::cast::AUDIO_EVENT, 10000));
  cast_environment->Logging()->AddRawEventSubscriber(
      video_event_subscriber.get());
  cast_environment->Logging()->AddRawEventSubscriber(
      audio_event_subscriber.get());

  // Subscribers for stats.
  scoped_ptr<media::cast::ReceiverTimeOffsetEstimatorImpl> offset_estimator(
      new media::cast::ReceiverTimeOffsetEstimatorImpl);
  cast_environment->Logging()->AddRawEventSubscriber(offset_estimator.get());
  scoped_ptr<media::cast::StatsEventSubscriber> video_stats_subscriber(
      new media::cast::StatsEventSubscriber(media::cast::VIDEO_EVENT,
                                            cast_environment->Clock(),
                                            offset_estimator.get()));
  scoped_ptr<media::cast::StatsEventSubscriber> audio_stats_subscriber(
      new media::cast::StatsEventSubscriber(media::cast::AUDIO_EVENT,
                                            cast_environment->Clock(),
                                            offset_estimator.get()));
  cast_environment->Logging()->AddRawEventSubscriber(
      video_stats_subscriber.get());
  cast_environment->Logging()->AddRawEventSubscriber(
      audio_stats_subscriber.get());

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

  const int logging_duration_seconds = 10;
  io_message_loop.message_loop_proxy()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WriteLogsToFileAndDestroySubscribers,
                 cast_environment,
                 base::Passed(&video_event_subscriber),
                 base::Passed(&audio_event_subscriber),
                 base::Passed(&video_log_file),
                 base::Passed(&audio_log_file)),
      base::TimeDelta::FromSeconds(logging_duration_seconds));

  io_message_loop.message_loop_proxy()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WriteStatsAndDestroySubscribers,
                 cast_environment,
                 base::Passed(&video_stats_subscriber),
                 base::Passed(&audio_stats_subscriber),
                 base::Passed(&offset_estimator)),
      base::TimeDelta::FromSeconds(logging_duration_seconds));

  fake_media_source->Start(cast_sender->audio_frame_input(),
                           cast_sender->video_frame_input());

  io_message_loop.Run();
  return 0;
}
