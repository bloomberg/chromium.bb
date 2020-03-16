// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>

#include <chrono>  // NOLINT
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstring>

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

#if defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)

// How often should the file position (media timestamp) be updated on the
// console?
static milliseconds kConsoleUpdateInterval{100};

// Plays the media file at a given path over and over again, transcoding and
// streaming its audio/video.
class LoopingFileSender final : public SimulatedAudioCapturer::Client,
                                public SimulatedVideoCapturer::Client {
 public:
  LoopingFileSender(TaskRunner* task_runner,
                    const char* path,
                    const IPEndpoint& remote_endpoint,
                    bool use_android_rtp_hack)
      : env_(&Clock::now, task_runner, IPEndpoint{IPAddress(), 0}),
        path_(path),
        packet_router_(&env_),
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
        next_task_(env_.now_function(), env_.task_runner()) {
    env_.set_remote_endpoint(remote_endpoint);
    OSP_LOG_INFO << "Streaming to " << remote_endpoint << "...";

    if (use_android_rtp_hack) {
      OSP_LOG_INFO << "Using RTP payload types for older Android TV receivers.";
    }

    next_task_.Schedule([this] { SendFileAgain(); }, Alarm::kImmediately);
  }

  ~LoopingFileSender() final = default;

 private:
  void SendFileAgain() {
    OSP_LOG_INFO << "Sending " << path_ << " (starts in one second)...";

    OSP_DCHECK_EQ(num_capturers_running_, 0);
    num_capturers_running_ = 2;
    capture_start_time_ = env_.now() + seconds(1);
    last_console_update_ = capture_start_time_ - kConsoleUpdateInterval;
    audio_capturer_.emplace(&env_, path_, audio_encoder_.num_channels(),
                            audio_encoder_.sample_rate(), capture_start_time_,
                            this);
    video_capturer_.emplace(&env_, path_, capture_start_time_, this);
  }

  void OnAudioData(const float* interleaved_samples,
                   int num_samples,
                   Clock::time_point capture_time) final {
    if ((capture_time - last_console_update_) >= kConsoleUpdateInterval) {
      const Clock::duration elapsed = capture_time - capture_start_time_;
      const auto seconds_part = duration_cast<seconds>(elapsed);
      const auto millis_part =
          duration_cast<milliseconds>(elapsed - seconds_part);
      const int bandwidth_kbps =
          std::max(packet_router_.ComputeNetworkBandwidth() / 1024, 0);
      fprintf(stdout,
              "At %01" PRId64
              ".%03ds in file (est. network bandwidth: %d kbps).          \r",
              static_cast<int64_t>(seconds_part.count()),
              static_cast<int>(millis_part.count()), bandwidth_kbps);
      fflush(stdout);
      last_console_update_ = capture_time;
    }

    audio_encoder_.EncodeAndSend(interleaved_samples, num_samples,
                                 capture_time);
  }

  void OnVideoFrame(const AVFrame& frame,
                    Clock::time_point capture_time) final {
    OSP_UNIMPLEMENTED();
  }

  void OnEndOfFile(SimulatedCapturer* capturer) final {
    OnCapturerHalted(capturer, "reached the end of the media stram.");
  }

  void OnError(SimulatedCapturer* capturer, std::string message) final {
    std::ostringstream what;
    what << "failed: " << message;
    OnCapturerHalted(capturer, what.str());
  }

  void OnCapturerHalted(SimulatedCapturer* capturer, const std::string& what) {
    if (capturer == &*audio_capturer_) {
      OSP_LOG_INFO << "The audio capturer has " << what;
    } else if (capturer == &*video_capturer_) {
      OSP_LOG_INFO << "The video capturer has " << what;
    } else {
      OSP_NOTREACHED();
    }
    --num_capturers_running_;
    if (num_capturers_running_ == 0) {
      next_task_.Schedule([this] { SendFileAgain(); }, Alarm::kImmediately);
    }
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

  Sender audio_sender_;
  Sender video_sender_;

  StreamingOpusEncoder audio_encoder_;

  int num_capturers_running_ = 0;
  Clock::time_point capture_start_time_{};
  Clock::time_point last_console_update_{};
  absl::optional<SimulatedAudioCapturer> audio_capturer_;
  absl::optional<SimulatedVideoCapturer> video_capturer_;

  Alarm next_task_;
};

#endif  // defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)

}  // namespace
}  // namespace cast
}  // namespace openscreen

namespace {

void LogUsage(const char* argv0) {
  constexpr char kExecutableTag[] = "argv[0]";
  constexpr char kUsageMessage[] = R"(
    usage: argv[0] <options> <media_file>

      --remote=addr[:port]
           Specify the destination (e.g., 192.168.1.22:9999 or [::1]:12345).

           Default if not set: 127.0.0.1 (and default Cast Streaming port 2344)

      --android-hack:
           Use the wrong RTP payload types, for compatibility with older Android
           TV receivers.

      --tracing: Enable performance tracing logging.
  )";
  std::string message = kUsageMessage;
  message.replace(message.find(kExecutableTag), strlen(kExecutableTag), argv0);
  OSP_LOG_ERROR << message;
}

}  // namespace

int main(int argc, char* argv[]) {
  using openscreen::Clock;
  using openscreen::ErrorOr;
  using openscreen::IPAddress;
  using openscreen::IPEndpoint;
  using openscreen::PlatformClientPosix;
  using openscreen::TaskRunnerImpl;

  openscreen::SetLogLevel(openscreen::LogLevel::kInfo);

  const struct option argument_options[] = {
      {"remote", required_argument, nullptr, 'r'},
      {"android-hack", no_argument, nullptr, 'a'},
      {"tracing", no_argument, nullptr, 't'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  IPEndpoint remote_endpoint{IPAddress::kV4LoopbackAddress,
                             openscreen::cast::kDefaultCastStreamingPort};
  [[maybe_unused]] bool use_android_rtp_hack = false;
  std::unique_ptr<openscreen::TextTraceLoggingPlatform> trace_logger;
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
      case 'a':
        use_android_rtp_hack = true;
        break;
      case 't':
        trace_logger = std::make_unique<openscreen::TextTraceLoggingPlatform>();
        break;
      case 'h':
        LogUsage(argv[0]);
        return 1;
    }
  }

  // The last command line argument must be the path to the file.
  const char* path = nullptr;
  if (optind < argc) {
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
    openscreen::cast::LoopingFileSender file_sender(
        task_runner, path, remote_endpoint, use_android_rtp_hack);
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
