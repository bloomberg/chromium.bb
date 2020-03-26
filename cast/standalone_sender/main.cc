// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>

#include <chrono>  // NOLINT
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "cast/streaming/constants.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/sender.h"
#include "cast/streaming/sender_packet_router.h"
#include "cast/streaming/session_config.h"
#include "cast/streaming/ssrc.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/impl/logging.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/text_trace_logging_platform.h"
#include "util/alarm.h"

#if defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)
#include "cast/standalone_sender/simulated_capturer.h"
#include "cast/standalone_sender/streaming_opus_encoder.h"
#include "cast/standalone_sender/streaming_vp8_encoder.h"
#endif

namespace openscreen {
namespace cast {
namespace {

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;

////////////////////////////////////////////////////////////////////////////////
// Sender Configuration
//
// The values defined here are constants that correspond to the standalone Cast
// Receiver app. In a production environment, these should ABSOLUTELY NOT be
// fixed! Instead a sender↔receiver OFFER/ANSWER exchange should establish them.

// In a production environment, this would start-out at some initial value
// appropriate to the networking environment, and then be adjusted by the
// application as: 1) the TYPE of the content changes (interactive, low-latency
// versus smooth, higher-latency buffered video watching); and 2) the networking
// environment reliability changes.
constexpr milliseconds kTargetPlayoutDelay = kDefaultTargetPlayoutDelay;

const SessionConfig kSampleAudioAnswerConfig{
    /* .sender_ssrc = */ 1,
    /* .receiver_ssrc = */ 2,
    /* .rtp_timebase = */ 48000,
    /* .channels = */ 2,
    /* .target_playout_delay */ kTargetPlayoutDelay,
    /* .aes_secret_key = */
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
     0x0c, 0x0d, 0x0e, 0x0f},
    /* .aes_iv_mask = */
    {0xf0, 0xe0, 0xd0, 0xc0, 0xb0, 0xa0, 0x90, 0x80, 0x70, 0x60, 0x50, 0x40,
     0x30, 0x20, 0x10, 0x00},
};

const SessionConfig kSampleVideoAnswerConfig{
    /* .sender_ssrc = */ 50001,
    /* .receiver_ssrc = */ 50002,
    /* .rtp_timebase = */ static_cast<int>(kVideoTimebase::den),
    /* .channels = */ 1,
    /* .target_playout_delay */ kTargetPlayoutDelay,
    /* .aes_secret_key = */
    {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
     0x1c, 0x1d, 0x1e, 0x1f},
    /* .aes_iv_mask = */
    {0xf1, 0xe1, 0xd1, 0xc1, 0xb1, 0xa1, 0x91, 0x81, 0x71, 0x61, 0x51, 0x41,
     0x31, 0x21, 0x11, 0x01},
};

// End of Sender Configuration.
////////////////////////////////////////////////////////////////////////////////

// What is the minimum amount of bandwidth required?
constexpr int kMinRequiredBitrate = 384 << 10;  // 384 kbps.

// What is the default maximum bitrate setting?
constexpr int kDefaultMaxBitrate = 5 << 20;  // 5 Mbps.

#if defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)

// Above what available bandwidth should the high-quality audio bitrate be used?
constexpr int kHighBandwidthThreshold = 5 << 20;  // 5 Mbps.

// How often should the file position (media timestamp) be updated on the
// console?
constexpr milliseconds kConsoleUpdateInterval{100};

// How often should the congestion control logic re-evaluate the target encode
// bitrates?
constexpr milliseconds kCongestionCheckInterval{500};

// Plays the media file at a given path over and over again, transcoding and
// streaming its audio/video.
class LoopingFileSender final : public SimulatedAudioCapturer::Client,
                                public SimulatedVideoCapturer::Client {
 public:
  LoopingFileSender(TaskRunner* task_runner,
                    const char* path,
                    const IPEndpoint& remote_endpoint,
                    int max_bitrate,
                    bool use_android_rtp_hack)
      : env_(&Clock::now, task_runner, IPEndpoint{IPAddress(), 0}),
        path_(path),
        packet_router_(&env_),
        max_bitrate_(max_bitrate),
        audio_sender_(&env_,
                      &packet_router_,
                      kSampleAudioAnswerConfig,
                      use_android_rtp_hack
                          ? RtpPayloadType::kAudioHackForAndroidTV
                          : RtpPayloadType::kAudioOpus),
        video_sender_(&env_,
                      &packet_router_,
                      kSampleVideoAnswerConfig,
                      use_android_rtp_hack
                          ? RtpPayloadType::kVideoHackForAndroidTV
                          : RtpPayloadType::kVideoVp8),
        audio_encoder_(kSampleAudioAnswerConfig.channels,
                       StreamingOpusEncoder::kDefaultCastAudioFramesPerSecond,
                       &audio_sender_),
        video_encoder_(StreamingVp8Encoder::Parameters{},
                       env_.task_runner(),
                       &video_sender_),
        next_task_(env_.now_function(), env_.task_runner()),
        console_update_task_(env_.now_function(), env_.task_runner()) {
    env_.set_remote_endpoint(remote_endpoint);
    OSP_LOG_INFO << "Streaming to " << remote_endpoint << "...";

    if (use_android_rtp_hack) {
      OSP_LOG_INFO << "Using RTP payload types for older Android TV receivers.";
    }

    OSP_LOG_INFO << "Max allowed media bitrate (audio + video) will be "
                 << max_bitrate_;
    bandwidth_being_utilized_ = max_bitrate_ / 2;
    UpdateEncoderBitrates();

    next_task_.Schedule([this] { SendFileAgain(); }, Alarm::kImmediately);
  }

  ~LoopingFileSender() final = default;

 private:
  void UpdateEncoderBitrates() {
    if (bandwidth_being_utilized_ >= kHighBandwidthThreshold) {
      audio_encoder_.UseHighQuality();
    } else {
      audio_encoder_.UseStandardQuality();
    }
    video_encoder_.SetTargetBitrate(bandwidth_being_utilized_ -
                                    audio_encoder_.GetBitrate());
  }

  void ControlForNetworkCongestion() {
    bandwidth_estimate_ = packet_router_.ComputeNetworkBandwidth();
    if (bandwidth_estimate_ > 0) {
      // Don't ever try to use *all* of the network bandwidth! However, don't go
      // below the absolute minimum requirement either.
      constexpr double kGoodNetworkCitizenFactor = 0.8;
      const int usable_bandwidth = std::max<int>(
          kGoodNetworkCitizenFactor * bandwidth_estimate_, kMinRequiredBitrate);

      // See "congestion control" discussion in the class header comments for
      // BandwidthEstimator.
      if (usable_bandwidth > bandwidth_being_utilized_) {
        constexpr double kConservativeIncrease = 1.1;
        bandwidth_being_utilized_ =
            std::min<int>(bandwidth_being_utilized_ * kConservativeIncrease,
                          usable_bandwidth);
      } else {
        bandwidth_being_utilized_ = usable_bandwidth;
      }

      // Repsect the user's maximum bitrate setting.
      bandwidth_being_utilized_ =
          std::min(bandwidth_being_utilized_, max_bitrate_);

      UpdateEncoderBitrates();
    } else {
      // There is no current bandwidth estimate. So, nothing should be adjusted.
    }

    next_task_.ScheduleFromNow([this] { ControlForNetworkCongestion(); },
                               kCongestionCheckInterval);
  }

  void SendFileAgain() {
    OSP_LOG_INFO << "Sending " << path_ << " (starts in one second)...";

    OSP_DCHECK_EQ(num_capturers_running_, 0);
    num_capturers_running_ = 2;
    capture_start_time_ = latest_frame_time_ = env_.now() + seconds(1);
    audio_capturer_.emplace(&env_, path_, audio_encoder_.num_channels(),
                            audio_encoder_.sample_rate(), capture_start_time_,
                            this);
    video_capturer_.emplace(&env_, path_, capture_start_time_, this);

    next_task_.ScheduleFromNow([this] { ControlForNetworkCongestion(); },
                               kCongestionCheckInterval);
    console_update_task_.Schedule([this] { UpdateStatusOnConsole(); },
                                  capture_start_time_);
  }

  void OnAudioData(const float* interleaved_samples,
                   int num_samples,
                   Clock::time_point capture_time) final {
    latest_frame_time_ = std::max(capture_time, latest_frame_time_);
    audio_encoder_.EncodeAndSend(interleaved_samples, num_samples,
                                 capture_time);
  }

  void OnVideoFrame(const AVFrame& av_frame,
                    Clock::time_point capture_time) final {
    latest_frame_time_ = std::max(capture_time, latest_frame_time_);
    StreamingVp8Encoder::VideoFrame frame{};
    frame.width = av_frame.width - av_frame.crop_left - av_frame.crop_right;
    frame.height = av_frame.height - av_frame.crop_top - av_frame.crop_bottom;
    frame.yuv_planes[0] = av_frame.data[0] + av_frame.crop_left +
                          av_frame.linesize[0] * av_frame.crop_top;
    frame.yuv_planes[1] = av_frame.data[1] + av_frame.crop_left / 2 +
                          av_frame.linesize[1] * av_frame.crop_top / 2;
    frame.yuv_planes[2] = av_frame.data[2] + av_frame.crop_left / 2 +
                          av_frame.linesize[2] * av_frame.crop_top / 2;
    for (int i = 0; i < 3; ++i) {
      frame.yuv_strides[i] = av_frame.linesize[i];
    }
    // TODO(miu): Add performance metrics visual overlay (based on Stats
    // callback).
    video_encoder_.EncodeAndSend(frame, capture_time, {});
  }

  void UpdateStatusOnConsole() {
    const Clock::duration elapsed = latest_frame_time_ - capture_start_time_;
    const auto seconds_part = duration_cast<seconds>(elapsed);
    const auto millis_part =
        duration_cast<milliseconds>(elapsed - seconds_part);
    // The control codes here attempt to erase the current line the cursor is
    // on, and then print out the updated status text. If the terminal does not
    // support simple ANSI escape codes, the following will still work, but
    // there might sometimes be old status lines not getting erased (i.e., just
    // partially overwritten).
    fprintf(stdout,
            "\r\x1b[2K\rAt %01" PRId64
            ".%03ds in file (est. network bandwidth: %d kbps). ",
            static_cast<int64_t>(seconds_part.count()),
            static_cast<int>(millis_part.count()), bandwidth_estimate_ / 1024);
    fflush(stdout);

    console_update_task_.ScheduleFromNow([this] { UpdateStatusOnConsole(); },
                                         kConsoleUpdateInterval);
  }

  void OnEndOfFile(SimulatedCapturer* capturer) final {
    OSP_LOG_INFO << "The " << ToTrackName(capturer)
                 << " capturer has reached the end of the media stream.";
    --num_capturers_running_;
    if (num_capturers_running_ == 0) {
      console_update_task_.Cancel();
      next_task_.Schedule([this] { SendFileAgain(); }, Alarm::kImmediately);
    }
  }

  void OnError(SimulatedCapturer* capturer, std::string message) final {
    OSP_LOG_ERROR << "The " << ToTrackName(capturer)
                  << " has failed: " << message;
    --num_capturers_running_;
    // If both fail, the application just pauses. This accounts for things like
    // "file not found" errors. However, if only one track fails, then keep
    // going.
  }

  const char* ToTrackName(SimulatedCapturer* capturer) const {
    const char* which;
    if (capturer == &*audio_capturer_) {
      which = "audio";
    } else if (capturer == &*video_capturer_) {
      which = "video";
    } else {
      OSP_NOTREACHED();
      which = "";
    }
    return which;
  }

  // Holds the required injected dependencies (clock, task runner) used for Cast
  // Streaming, and owns the UDP socket over which all communications occur with
  // the remote's Receivers.
  Environment env_;

  // The path to the media file to stream over and over.
  const char* const path_;

  // The packet router allows both the Audio Sender and the Video Sender to
  // share the same UDP socket.
  SenderPacketRouter packet_router_;

  const int max_bitrate_;  // Passed by the user on the command line.
  int bandwidth_estimate_ = 0;
  int bandwidth_being_utilized_;

  Sender audio_sender_;
  Sender video_sender_;

  StreamingOpusEncoder audio_encoder_;
  StreamingVp8Encoder video_encoder_;

  int num_capturers_running_ = 0;
  Clock::time_point capture_start_time_{};
  Clock::time_point latest_frame_time_{};
  absl::optional<SimulatedAudioCapturer> audio_capturer_;
  absl::optional<SimulatedVideoCapturer> video_capturer_;

  Alarm next_task_;
  Alarm console_update_task_;
};

#endif  // defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)

IPEndpoint GetDefaultEndpoint() {
  return IPEndpoint{IPAddress::kV4LoopbackAddress, kDefaultCastStreamingPort};
}

void LogUsage(const char* argv0) {
  const char kUsageMessageFormat[] = R"(
    usage: %s <options> <media_file>

      --remote=addr[:port]
           Specify the destination (e.g., 192.168.1.22:9999 or [::1]:12345).

           Default if not set: %s

      --max-bitrate=N
           Specifies the maximum bits per second for the media streams.

           Default if not set: %d.

      --android-hack:
           Use the wrong RTP payload types, for compatibility with older Android
           TV receivers.

      --tracing: Enable performance tracing logging.
  )";
  // TODO(https://crbug.com/openscreen/122): absl::StreamFormat() would be much
  // cleaner here. For example, all the code here could be replaced with:
  //
  // OSP_LOG_ERROR << absl::StreamFormat(kUsageMessageFormat, argv0,
  //                                     absl::FromatStreamed(endpoint),
  //                                     kDefaultMaxBitrate);
  std::string endpoint;
  {
    std::ostringstream oss;
    oss << GetDefaultEndpoint();
    endpoint = oss.str();
  }
  const int formatted_length_with_nul =
      snprintf(nullptr, 0, kUsageMessageFormat, argv0, endpoint.c_str(),
               kDefaultMaxBitrate) +
      1;
  const std::unique_ptr<char[]> usage_cstr(new char[formatted_length_with_nul]);
  snprintf(usage_cstr.get(), formatted_length_with_nul, kUsageMessageFormat,
           argv0, endpoint.c_str(), kDefaultMaxBitrate);
  OSP_LOG_ERROR << usage_cstr.get();
}

int StandaloneSenderMain(int argc, char* argv[]) {
  SetLogLevel(LogLevel::kInfo);

  const struct option argument_options[] = {
      {"remote", required_argument, nullptr, 'r'},
      {"max-bitrate", required_argument, nullptr, 'm'},
      {"android-hack", no_argument, nullptr, 'a'},
      {"tracing", no_argument, nullptr, 't'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  IPEndpoint remote_endpoint = GetDefaultEndpoint();
  [[maybe_unused]] bool use_android_rtp_hack = false;
  [[maybe_unused]] int max_bitrate = kDefaultMaxBitrate;
  std::unique_ptr<TextTraceLoggingPlatform> trace_logger;
  int ch = -1;
  while ((ch = getopt_long(argc, argv, "r:ath", argument_options, nullptr)) !=
         -1) {
    switch (ch) {
      case 'r': {
        const ErrorOr<IPEndpoint> parsed_endpoint = IPEndpoint::Parse(optarg);
        if (parsed_endpoint.is_value()) {
          remote_endpoint = parsed_endpoint.value();
        } else {
          const ErrorOr<IPAddress> parsed_address = IPAddress::Parse(optarg);
          if (parsed_address.is_value()) {
            remote_endpoint.address = parsed_address.value();
          } else {
            OSP_LOG_ERROR << "Invalid --remote specified: " << optarg;
            LogUsage(argv[0]);
            return 1;
          }
        }
        break;
      }
      case 'm':
        max_bitrate = atoi(optarg);
        if (max_bitrate < kMinRequiredBitrate) {
          OSP_LOG_ERROR << "Invalid --max-bitrate specified: " << optarg
                        << " is less than " << kMinRequiredBitrate;
          LogUsage(argv[0]);
          return 1;
        }
        break;
      case 'a':
        use_android_rtp_hack = true;
        break;
      case 't':
        trace_logger = std::make_unique<TextTraceLoggingPlatform>();
        break;
      case 'h':
        LogUsage(argv[0]);
        return 1;
    }
  }

  // The last command line argument must be the path to the file.
  const char* path = nullptr;
  if (optind == (argc - 1)) {
    path = argv[optind];
  }

  if (!path || !remote_endpoint.port) {
    LogUsage(argv[0]);
    return 1;
  }

#if defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)

  auto* const task_runner = new TaskRunnerImpl(&Clock::now);
  PlatformClientPosix::Create(Clock::duration{50}, Clock::duration{50},
                              std::unique_ptr<TaskRunnerImpl>(task_runner));

  {
    LoopingFileSender file_sender(task_runner, path, remote_endpoint,
                                  max_bitrate, use_android_rtp_hack);
    // Run the event loop until SIGINT (e.g., CTRL-C at the console) or SIGTERM
    // are signaled.
    task_runner->RunUntilSignaled();
  }

  PlatformClientPosix::ShutDown();

#else

  OSP_LOG_INFO
      << "It compiled! However, you need to configure the build to point to "
         "external libraries in order to build a useful app.";

#endif  // defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)

  return 0;
}

}  // namespace
}  // namespace cast
}  // namespace openscreen

int main(int argc, char* argv[]) {
  return openscreen::cast::StandaloneSenderMain(argc, argv);
}
