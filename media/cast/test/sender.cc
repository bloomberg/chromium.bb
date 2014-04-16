// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test application that simulates a cast sender - Data can be either generated
// or read from a file.

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "media/base/media.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
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
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/in_memory_url_protocol.h"
#include "ui/gfx/size.h"

namespace {
static const int kAudioChannels = 2;
static const int kAudioSamplingFrequency = 48000;
static const int kSoundFrequency = 1234;  // Frequency of sinusoid wave.
static const float kSoundVolume = 0.5f;
static const int kAudioFrameMs = 10;  // Each audio frame is exactly 10ms.

// The max allowed size of serialized log.
const int kMaxSerializedLogBytes = 10 * 1000 * 1000;

const char kSwitchAddress[] = "address";
const char kSwitchPort[] = "port";
const char kSwitchSourceFile[] = "source-file";

}  // namespace

namespace media {
namespace cast {

AudioSenderConfig GetAudioSenderConfig() {
  AudioSenderConfig audio_config;

  audio_config.rtcp_c_name = "audio_sender@a.b.c.d";

  VLOG(0) << "Using OPUS 48Khz stereo at 64kbit/s";
  audio_config.use_external_encoder = false;
  audio_config.frequency = kAudioSamplingFrequency;
  audio_config.channels = kAudioChannels;
  audio_config.bitrate = 64000;
  audio_config.codec = transport::kOpus;
  audio_config.sender_ssrc = 1;
  audio_config.incoming_feedback_ssrc = 2;
  audio_config.rtp_config.payload_type = 127;
  audio_config.rtp_config.max_delay_ms = 300;
  return audio_config;
}

VideoSenderConfig GetVideoSenderConfig() {
  VideoSenderConfig video_config;

  video_config.rtcp_c_name = "video_sender@a.b.c.d";
  video_config.use_external_encoder = false;

  VLOG(0) << "Using VP8 at 30 fps";

  // Resolution.
  video_config.width = 1280;
  video_config.height = 720;
  video_config.max_frame_rate = 30;

  // Bitrates.
  video_config.start_bitrate = 50000;
  video_config.max_bitrate = 2500000;
  video_config.min_bitrate = 100000;

  // Codec.
  video_config.codec = transport::kVp8;
  video_config.max_number_of_video_buffers_used = 1;
  video_config.number_of_encode_threads = 2;

  // Quality options.
  video_config.min_qp = 4;
  video_config.max_qp = 40;

  // SSRCs and payload type. Don't change them.
  video_config.sender_ssrc = 11;
  video_config.incoming_feedback_ssrc = 12;
  video_config.rtp_config.payload_type = 96;
  video_config.rtp_config.max_delay_ms = 300;
  return video_config;
}

void AVFreeFrame(AVFrame* frame) { avcodec_free_frame(&frame); }

class SendProcess {
 public:
  SendProcess(scoped_refptr<base::SingleThreadTaskRunner> thread_proxy,
              base::TickClock* clock,
              const VideoSenderConfig& video_config,
              scoped_refptr<AudioFrameInput> audio_frame_input,
              scoped_refptr<VideoFrameInput> video_frame_input)
      : test_app_thread_proxy_(thread_proxy),
        video_config_(video_config),
        audio_frame_input_(audio_frame_input),
        video_frame_input_(video_frame_input),
        synthetic_count_(0),
        clock_(clock),
        audio_frame_count_(0),
        video_frame_count_(0),
        weak_factory_(this),
        av_format_context_(NULL),
        audio_stream_index_(-1),
        video_stream_index_(-1) {
    audio_bus_factory_.reset(new TestAudioBusFactory(kAudioChannels,
                                                     kAudioSamplingFrequency,
                                                     kSoundFrequency,
                                                     kSoundVolume));

    // Load source file and prepare FFmpeg demuxer.
    base::FilePath source_path =
        CommandLine::ForCurrentProcess()->GetSwitchValuePath(kSwitchSourceFile);
    if (source_path.empty())
      return;

    LOG(INFO) << "Source: " << source_path.value();
    if (!file_data_.Initialize(source_path)) {
      LOG(ERROR) << "Cannot load file.";
      return;
    }
    protocol_.reset(
        new InMemoryUrlProtocol(file_data_.data(), file_data_.length(), false));
    glue_.reset(new FFmpegGlue(protocol_.get()));

    if (!glue_->OpenContext()) {
      LOG(ERROR) << "Cannot open file.";
      return;
    }

    // AVFormatContext is owned by the glue.
    av_format_context_ = glue_->format_context();
    if (avformat_find_stream_info(av_format_context_, NULL) < 0) {
      LOG(ERROR) << "Cannot find stream information.";
      return;
    }

    // Prepare FFmpeg decoders.
    for (unsigned int i = 0; i < av_format_context_->nb_streams; ++i) {
      AVStream* av_stream = av_format_context_->streams[i];
      AVCodecContext* av_codec_context = av_stream->codec;
      AVCodec* av_codec = avcodec_find_decoder(av_codec_context->codec_id);

      if (!av_codec) {
        LOG(ERROR) << "Cannot find decoder for the codec: "
                   << av_codec_context->codec_id;
        continue;
      }

      // Number of threads for decoding.
      av_codec_context->thread_count = 2;
      av_codec_context->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;

      if (avcodec_open2(av_codec_context, av_codec, NULL) < 0) {
        LOG(ERROR) << "Cannot open AVCodecContext for the codec: "
                   << av_codec_context->codec_id;
        return;
      }

      if (av_codec->type == AVMEDIA_TYPE_AUDIO) {
        if (audio_stream_index_ != -1) {
          LOG(WARNING) << "Found multiple audio streams.";
        }
        audio_stream_index_ = static_cast<int>(i);
        LOG(INFO) << "Source file has audio.";
      } else if (av_codec->type == AVMEDIA_TYPE_VIDEO) {
        VideoFrame::Format format =
            PixelFormatToVideoFormat(av_codec_context->pix_fmt);
        if (format != VideoFrame::YV12) {
          LOG(ERROR) << "Cannot handle non YV12 video format: " << format;
          return;
        }
        if (video_stream_index_ != -1) {
          LOG(WARNING) << "Found multiple video streams.";
        }
        video_stream_index_ = static_cast<int>(i);
        LOG(INFO) << "Source file has video.";
      } else {
        LOG(ERROR) << "Unknown stream type; ignore.";
      }
    }

    Rewind();
  }

  ~SendProcess() {
  }

  void SendNextFrame() {
    gfx::Size size(video_config_.width, video_config_.height);
    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::CreateBlackFrame(size);
    if (is_transcoding_video()) {
      scoped_refptr<VideoFrame> decoded_frame = DecodeOneVideoFrame();
      media::CopyPlane(VideoFrame::kYPlane,
                       decoded_frame->data(VideoFrame::kYPlane),
                       decoded_frame->stride(VideoFrame::kYPlane),
                       decoded_frame->rows(VideoFrame::kYPlane),
                       video_frame);
      media::CopyPlane(VideoFrame::kUPlane,
                       decoded_frame->data(VideoFrame::kUPlane),
                       decoded_frame->stride(VideoFrame::kUPlane),
                       decoded_frame->rows(VideoFrame::kUPlane),
                       video_frame);
      media::CopyPlane(VideoFrame::kVPlane,
                       decoded_frame->data(VideoFrame::kVPlane),
                       decoded_frame->stride(VideoFrame::kVPlane),
                       decoded_frame->rows(VideoFrame::kVPlane),
                       video_frame);
    } else {
      PopulateVideoFrame(video_frame, synthetic_count_);
    }
    ++synthetic_count_;

    base::TimeTicks now = clock_->NowTicks();
    if (start_time_.is_null())
      start_time_ = now;

    // Note: We don't care about the framerate of the original file.
    // We want the receiver to play it at the rate we desire, i.e. 30FPS.

    // After submitting the current frame update video time for the next
    // frame.
    base::TimeDelta video_time = VideoFrameTime(video_frame_count_);
    video_frame->set_timestamp(video_time);
    video_frame_input_->InsertRawVideoFrame(video_frame,
                                            start_time_ + video_time);
    video_time = VideoFrameTime(++video_frame_count_);

    base::TimeDelta audio_time = AudioFrameTime(audio_frame_count_);

    // Send just enough audio data to match next video frame's time.
    while (audio_time < video_time) {
      audio_frame_input_->InsertAudio(
          audio_bus_factory_->NextAudioBus(
              base::TimeDelta::FromMilliseconds(kAudioFrameMs)),
          start_time_ + audio_time);
      audio_time = AudioFrameTime(++audio_frame_count_);
    }

    // This is the time since the stream started.
    const base::TimeDelta elapsed_time = now - start_time_;

    // Handle the case when decoding or frame generation cannot keep up.
    // Move the time ahead to match the next frame.
    while (video_time < elapsed_time) {
      LOG(WARNING) << "Skipping one frame.";
      video_time = VideoFrameTime(++video_frame_count_);
    }

    test_app_thread_proxy_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SendProcess::SendNextFrame, weak_factory_.GetWeakPtr()),
        video_time - elapsed_time);
  }

 private:
  bool is_transcoding_audio() { return audio_stream_index_ >= 0; }
  bool is_transcoding_video() { return video_stream_index_ >= 0; }

  // Helper methods to compute timestamps for the frame number specified.
  base::TimeDelta VideoFrameTime(int frame_number) {
    return frame_number * base::TimeDelta::FromSeconds(1) /
        video_config_.max_frame_rate;
  }

  base::TimeDelta AudioFrameTime(int frame_number) {
    return frame_number * base::TimeDelta::FromMilliseconds(kAudioFrameMs);
  }

  // Go to the beginning of the stream.
  void Rewind() {
    CHECK(av_seek_frame(av_format_context_, -1, 0, AVSEEK_FLAG_BACKWARD) >= 0)
        << "Failed to rewind to the beginning.";
  }

  // Call FFmpeg to fetch one packet.
  ScopedAVPacket DemuxOnePacket(bool* audio) {
    ScopedAVPacket packet(new AVPacket());
    if (av_read_frame(av_format_context_, packet.get()) < 0) {
      LOG(ERROR) << "Failed to read one AVPacket";
      packet.reset();
      return packet.Pass();
    }

    int stream_index = static_cast<int>(packet->stream_index);
    if (stream_index == audio_stream_index_) {
      *audio = true;
    } else if (stream_index == video_stream_index_) {
      *audio = false;
    } else {
      // Ignore unknown packet.
      LOG(INFO) << "Unknown packet.";
      packet.reset();
    }
    return packet.Pass();
  }

  scoped_refptr<VideoFrame> DecodeOneVideoFrame() {
    // Read the stream until one video frame can be decoded.
    while (true) {
      bool audio = false;
      ScopedAVPacket packet = DemuxOnePacket(&audio);
      if (!packet) {
        LOG(INFO) << "End of stream; Rewind.";
        Rewind();
        continue;
      }

      if (audio) {
        // TODO(hclam): Decode audio packets.
        continue;
      }

      // Video.
      int got_picture = 0;
      AVFrame* avframe = av_frame_alloc();
      avcodec_get_frame_defaults(avframe);
      av_video_context()->reordered_opaque = packet->pts;
      CHECK(avcodec_decode_video2(
                av_video_context(), avframe, &got_picture, packet.get()) >= 0)
          << "Video decode error.";
      if (!got_picture) {
        continue;
      }

      gfx::Size size(av_video_context()->width, av_video_context()->height);
      return VideoFrame::WrapExternalYuvData(media::VideoFrame::YV12,
                                             size,
                                             gfx::Rect(size),
                                             size,
                                             avframe->linesize[0],
                                             avframe->linesize[1],
                                             avframe->linesize[2],
                                             avframe->data[0],
                                             avframe->data[1],
                                             avframe->data[2],
                                             base::TimeDelta(),
                                             base::Bind(&AVFreeFrame, avframe));
    }
  }

  void SendVideoFrameOnTime(scoped_refptr<media::VideoFrame> video_frame) {
  }

  AVStream* av_audio_stream() {
    return av_format_context_->streams[audio_stream_index_];
  }
  AVStream* av_video_stream() {
    return av_format_context_->streams[video_stream_index_];
  }
  AVCodecContext* av_audio_context() { return av_audio_stream()->codec; }
  AVCodecContext* av_video_context() { return av_video_stream()->codec; }

  scoped_refptr<base::SingleThreadTaskRunner> test_app_thread_proxy_;
  const VideoSenderConfig video_config_;
  const scoped_refptr<AudioFrameInput> audio_frame_input_;
  const scoped_refptr<VideoFrameInput> video_frame_input_;
  uint8 synthetic_count_;
  base::TickClock* const clock_;  // Not owned by this class.
  base::TimeTicks start_time_;
  int audio_frame_count_;  // Each audio frame is exactly 10ms.
  int video_frame_count_;
  scoped_ptr<TestAudioBusFactory> audio_bus_factory_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<SendProcess> weak_factory_;

  base::MemoryMappedFile file_data_;
  scoped_ptr<InMemoryUrlProtocol> protocol_;
  scoped_ptr<FFmpegGlue> glue_;
  AVFormatContext* av_format_context_;

  int audio_stream_index_;
  int video_stream_index_;

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

void WriteLogsToFileAndStopSubscribing(
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
  media::cast::FrameEventMap frame_events;
  media::cast::PacketEventMap packet_events;
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
  int remote_port = 2344;
  base::StringToInt(cmd->GetSwitchValueASCII(kSwitchPort), &remote_port);

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

  const int logging_duration_seconds = 300;
  io_message_loop.message_loop_proxy()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WriteLogsToFileAndStopSubscribing,
                 cast_environment,
                 base::Passed(&video_event_subscriber),
                 base::Passed(&audio_event_subscriber),
                 base::Passed(&video_log_file),
                 base::Passed(&audio_log_file)),
      base::TimeDelta::FromSeconds(logging_duration_seconds));

  test_thread.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&media::cast::SendProcess::SendNextFrame,
                 base::Unretained(send_process.get())));

  io_message_loop.Run();

  return 0;
}
