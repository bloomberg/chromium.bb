// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "media/base/audio_bus.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/in_process_receiver.h"
#include "media/cast/test/utility/input_builder.h"
#include "media/cast/test/utility/standalone_cast_environment.h"
#include "media/cast/transport/transport/udp_transport.h"
#include "net/base/net_util.h"

#if defined(OS_LINUX)
#include "media/cast/test/linux_output_window.h"
#endif  // OS_LINUX

namespace media {
namespace cast {

// Settings chosen to match default sender settings.
#define DEFAULT_SEND_PORT "0"
#define DEFAULT_RECEIVE_PORT "2344"
#define DEFAULT_SEND_IP "0.0.0.0"
#define DEFAULT_RESTART "0"
#define DEFAULT_AUDIO_FEEDBACK_SSRC "2"
#define DEFAULT_AUDIO_INCOMING_SSRC "1"
#define DEFAULT_AUDIO_PAYLOAD_TYPE "127"
#define DEFAULT_VIDEO_FEEDBACK_SSRC "12"
#define DEFAULT_VIDEO_INCOMING_SSRC "11"
#define DEFAULT_VIDEO_PAYLOAD_TYPE "96"
#define DEFAULT_VIDEO_CODEC_WIDTH "640"
#define DEFAULT_VIDEO_CODEC_HEIGHT "480"
#define DEFAULT_VIDEO_CODEC_BITRATE "2000"

#if defined(OS_LINUX)
const int kVideoWindowWidth = 1280;
const int kVideoWindowHeight = 720;
#endif  // OS_LINUX

void GetPorts(int* tx_port, int* rx_port) {
  test::InputBuilder tx_input(
      "Enter send port.", DEFAULT_SEND_PORT, 1, INT_MAX);
  *tx_port = tx_input.GetIntInput();

  test::InputBuilder rx_input(
      "Enter receive port.", DEFAULT_RECEIVE_PORT, 1, INT_MAX);
  *rx_port = rx_input.GetIntInput();
}

std::string GetIpAddress(const std::string display_text) {
  test::InputBuilder input(display_text, DEFAULT_SEND_IP, INT_MIN, INT_MAX);
  std::string ip_address = input.GetStringInput();
  // Ensure IP address is either the default value or in correct form.
  while (ip_address != DEFAULT_SEND_IP &&
         std::count(ip_address.begin(), ip_address.end(), '.') != 3) {
    ip_address = input.GetStringInput();
  }
  return ip_address;
}

void GetSsrcs(AudioReceiverConfig* audio_config) {
  test::InputBuilder input_tx(
      "Choose audio sender SSRC.", DEFAULT_AUDIO_FEEDBACK_SSRC, 1, INT_MAX);
  audio_config->feedback_ssrc = input_tx.GetIntInput();

  test::InputBuilder input_rx(
      "Choose audio receiver SSRC.", DEFAULT_AUDIO_INCOMING_SSRC, 1, INT_MAX);
  audio_config->incoming_ssrc = input_rx.GetIntInput();
}

void GetSsrcs(VideoReceiverConfig* video_config) {
  test::InputBuilder input_tx(
      "Choose video sender SSRC.", DEFAULT_VIDEO_FEEDBACK_SSRC, 1, INT_MAX);
  video_config->feedback_ssrc = input_tx.GetIntInput();

  test::InputBuilder input_rx(
      "Choose video receiver SSRC.", DEFAULT_VIDEO_INCOMING_SSRC, 1, INT_MAX);
  video_config->incoming_ssrc = input_rx.GetIntInput();
}

void GetPayloadtype(AudioReceiverConfig* audio_config) {
  test::InputBuilder input("Choose audio receiver payload type.",
                           DEFAULT_AUDIO_PAYLOAD_TYPE,
                           96,
                           127);
  audio_config->rtp_payload_type = input.GetIntInput();
}

AudioReceiverConfig GetAudioReceiverConfig() {
  AudioReceiverConfig audio_config = GetDefaultAudioReceiverConfig();
  GetSsrcs(&audio_config);
  GetPayloadtype(&audio_config);
  return audio_config;
}

void GetPayloadtype(VideoReceiverConfig* video_config) {
  test::InputBuilder input("Choose video receiver payload type.",
                           DEFAULT_VIDEO_PAYLOAD_TYPE,
                           96,
                           127);
  video_config->rtp_payload_type = input.GetIntInput();
}

VideoReceiverConfig GetVideoReceiverConfig() {
  VideoReceiverConfig video_config = GetDefaultVideoReceiverConfig();
  GetSsrcs(&video_config);
  GetPayloadtype(&video_config);
  return video_config;
}

// An InProcessReceiver that renders video frames to a LinuxOutputWindow.  While
// it does receive audio frames, it does not play them.
class ReceiverDisplay : public InProcessReceiver {
 public:
  ReceiverDisplay(const scoped_refptr<CastEnvironment>& cast_environment,
                  const net::IPEndPoint& local_end_point,
                  const net::IPEndPoint& remote_end_point,
                  const AudioReceiverConfig& audio_config,
                  const VideoReceiverConfig& video_config)
      : InProcessReceiver(cast_environment,
                          local_end_point,
                          remote_end_point,
                          audio_config,
                          video_config),
#if defined(OS_LINUX)
        render_(0, 0, kVideoWindowWidth, kVideoWindowHeight, "Cast_receiver"),
#endif  // OS_LINUX
        last_playout_time_(),
        last_render_time_() {
  }

  virtual ~ReceiverDisplay() {}

 protected:
  virtual void OnVideoFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                            const base::TimeTicks& render_time,
                            bool is_continuous) OVERRIDE {
#ifdef OS_LINUX
    render_.RenderFrame(video_frame);
#endif  // OS_LINUX
    // Print out the delta between frames.
    if (!last_render_time_.is_null()) {
      base::TimeDelta time_diff = render_time - last_render_time_;
      VLOG(2) << "Size = " << video_frame->coded_size().ToString()
              << "; RenderDelay[mS] =  " << time_diff.InMilliseconds();
    }
    last_render_time_ = render_time;
  }

  virtual void OnAudioFrame(scoped_ptr<AudioBus> audio_frame,
                            const base::TimeTicks& playout_time,
                            bool is_continuous) OVERRIDE {
    // For audio just print the playout delta between audio frames.
    if (!last_playout_time_.is_null()) {
      base::TimeDelta time_diff = playout_time - last_playout_time_;
      VLOG(2) << "SampleRate = " << audio_config().frequency
              << "; PlayoutDelay[mS] =  " << time_diff.InMilliseconds();
    }
    last_playout_time_ = playout_time;
  }

#ifdef OS_LINUX
  test::LinuxOutputWindow render_;
#endif  // OS_LINUX
  base::TimeTicks last_playout_time_;
  base::TimeTicks last_render_time_;
};

}  // namespace cast
}  // namespace media

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());

  scoped_refptr<media::cast::CastEnvironment> cast_environment(
      new media::cast::StandaloneCastEnvironment);

  media::cast::AudioReceiverConfig audio_config =
      media::cast::GetAudioReceiverConfig();
  media::cast::VideoReceiverConfig video_config =
      media::cast::GetVideoReceiverConfig();

  int remote_port, local_port;
  media::cast::GetPorts(&remote_port, &local_port);
  if (!local_port) {
    LOG(ERROR) << "Invalid local port.";
    return 1;
  }

  std::string remote_ip_address = media::cast::GetIpAddress("Enter remote IP.");
  std::string local_ip_address = media::cast::GetIpAddress("Enter local IP.");
  net::IPAddressNumber remote_ip_number;
  net::IPAddressNumber local_ip_number;

  if (!net::ParseIPLiteralToNumber(remote_ip_address, &remote_ip_number)) {
    LOG(ERROR) << "Invalid remote IP address.";
    return 1;
  }

  if (!net::ParseIPLiteralToNumber(local_ip_address, &local_ip_number)) {
    LOG(ERROR) << "Invalid local IP address.";
    return 1;
  }

  net::IPEndPoint remote_end_point(remote_ip_number, remote_port);
  net::IPEndPoint local_end_point(local_ip_number, local_port);

  media::cast::ReceiverDisplay* const receiver_display =
      new media::cast::ReceiverDisplay(cast_environment,
                                       local_end_point,
                                       remote_end_point,
                                       audio_config,
                                       video_config);
  receiver_display->Start();

  base::MessageLoop().Run();  // Run forever (i.e., until SIGTERM).
  NOTREACHED();
  return 0;
}
