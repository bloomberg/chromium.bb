// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <string>

#include "base/at_exit.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/test/transport/transport.h"
#include "media/cast/test/utility/input_helper.h"

#if defined(OS_LINUX)
#include "media/cast/test/linux_output_window.h"
#endif  // OS_LINUX

namespace media {
namespace cast {

#define DEFAULT_SEND_PORT "2346"
#define DEFAULT_RECEIVE_PORT "2344"
#define DEFAULT_SEND_IP "127.0.0.1"
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

static const int kAudioSamplingFrequency = 48000;
#if defined(OS_LINUX)
const int kVideoWindowWidth = 1280;
const int kVideoWindowHeight = 720;
#endif  // OS_LINUX
static const int kFrameTimerMs = 33;


void GetPorts(int* tx_port, int* rx_port) {
  test::InputBuilder tx_input("Enter send port.",
      DEFAULT_SEND_PORT, 1, INT_MAX);
  *tx_port = tx_input.GetIntInput();

  test::InputBuilder rx_input("Enter receive port.",
      DEFAULT_RECEIVE_PORT, 1, INT_MAX);
  *rx_port = rx_input.GetIntInput();
}

std::string GetIpAddress() {
 test::InputBuilder input("Enter destination IP.",DEFAULT_SEND_IP,
      INT_MIN, INT_MAX);
  std::string ip_address = input.GetStringInput();
  // Ensure correct form:
  while (std::count(ip_address.begin(), ip_address.end(), '.') != 3) {
    ip_address = input.GetStringInput();
  }
  return ip_address;
}

void GetSsrcs(AudioReceiverConfig* audio_config) {
  test::InputBuilder input_tx("Choose audio sender SSRC.",
      DEFAULT_AUDIO_FEEDBACK_SSRC, 1, INT_MAX);
  audio_config->feedback_ssrc = input_tx.GetIntInput();

  test::InputBuilder input_rx("Choose audio receiver SSRC.",
      DEFAULT_AUDIO_INCOMING_SSRC, 1, INT_MAX);
  audio_config->incoming_ssrc = input_tx.GetIntInput();
}

void GetSsrcs(VideoReceiverConfig* video_config) {
  test::InputBuilder input_tx("Choose video sender SSRC.",
      DEFAULT_VIDEO_FEEDBACK_SSRC, 1, INT_MAX);
  video_config->feedback_ssrc = input_tx.GetIntInput();

  test::InputBuilder input_rx("Choose video receiver SSRC.",
      DEFAULT_VIDEO_INCOMING_SSRC, 1, INT_MAX);
  video_config->incoming_ssrc = input_rx.GetIntInput();
}

void GetPayloadtype(AudioReceiverConfig* audio_config) {
  test::InputBuilder input("Choose audio receiver payload type.",
      DEFAULT_AUDIO_PAYLOAD_TYPE, 96, 127);
  audio_config->rtp_payload_type = input.GetIntInput();
}

AudioReceiverConfig GetAudioReceiverConfig() {
  AudioReceiverConfig audio_config;

  GetSsrcs(&audio_config);
  GetPayloadtype(&audio_config);

  audio_config.rtcp_c_name = "audio_receiver@a.b.c.d";

  VLOG(1) << "Using OPUS 48Khz stereo";
  audio_config.use_external_decoder = false;
  audio_config.frequency = 48000;
  audio_config.channels = 2;
  audio_config.codec = kOpus;
  return audio_config;
}

void GetPayloadtype(VideoReceiverConfig* video_config) {
  test::InputBuilder input("Choose video receiver payload type.",
      DEFAULT_VIDEO_PAYLOAD_TYPE, 96, 127);
  video_config->rtp_payload_type = input.GetIntInput();
}

VideoReceiverConfig GetVideoReceiverConfig() {
  VideoReceiverConfig video_config;

  GetSsrcs(&video_config);
  GetPayloadtype(&video_config);

  video_config.rtcp_c_name = "video_receiver@a.b.c.d";

  video_config.use_external_decoder = false;

  VLOG(1) << "Using VP8";
  video_config.codec = kVp8;
  return video_config;
}


class ReceiveProcess {
 public:
  ReceiveProcess(scoped_refptr<CastEnvironment> cast_environment,
                 scoped_refptr<FrameReceiver> frame_receiver)
      :  cast_environment_(cast_environment),
         frame_receiver_(frame_receiver),
#if defined(OS_LINUX)
         render_(0, 0, kVideoWindowWidth, kVideoWindowHeight, "Cast_receiver"),
#endif // OS_LINUX
         last_playout_time_(),
         weak_factory_(this) {}

  void Start() {
    GetFrame();
  }

 private:
  void DisplayFrame(scoped_ptr<I420VideoFrame> frame,
      const base::TimeTicks& render_time) {
#ifdef OS_LINUX
    render_.RenderFrame(*frame);
#endif // OS_LINUX
    GetFrame();
  }

  void ReceiveAudioFrame(scoped_ptr<PcmAudioFrame> audio_frame,
                         const base::TimeTicks& playout_time) {
    // For audio just print the playout delta between audio frames.
    if (!last_playout_time_.is_null()){
        base::TimeDelta time_diff = playout_time - last_playout_time_;
        VLOG(0) << " PlayoutDelay[mS] =  " << time_diff.InMilliseconds();
    }
    last_playout_time_ = playout_time;
    GetFrame();
  }

  void GetFrame() {
    frame_receiver_->GetRawVideoFrame(
        base::Bind(&ReceiveProcess::DisplayFrame, weak_factory_.GetWeakPtr()));
    int num_10ms_blocks = kFrameTimerMs / 10;
    frame_receiver_->GetRawAudioFrame(num_10ms_blocks, kAudioSamplingFrequency,
        base::Bind(&ReceiveProcess::ReceiveAudioFrame,
        weak_factory_.GetWeakPtr()));
  }

  const scoped_refptr<CastEnvironment> cast_environment_;
  const  scoped_refptr<FrameReceiver> frame_receiver_;
#ifdef OS_LINUX
  test::LinuxOutputWindow render_;
#endif // OS_LINUX
  base::TimeTicks last_playout_time_;
  base::WeakPtrFactory<ReceiveProcess> weak_factory_;
};

}  // namespace cast
}  // namespace media

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::MessageLoopForIO main_message_loop;
  VLOG(1) << "Cast Receiver";

  // Set up environment.
  base::DefaultTickClock clock;
  scoped_refptr<base::SequencedTaskRunner>
      task_runner(main_message_loop.message_loop_proxy());
  scoped_refptr<media::cast::CastEnvironment> cast_environment(new
      media::cast::CastEnvironment(&clock,
      task_runner, NULL, task_runner, NULL, task_runner));

  media::cast::AudioReceiverConfig audio_config =
      media::cast::GetAudioReceiverConfig();
  media::cast::VideoReceiverConfig video_config =
      media::cast::GetVideoReceiverConfig();

  scoped_ptr<media::cast::test::Transport> transport(
      new media::cast::test::Transport(cast_environment));
  scoped_ptr<media::cast::CastReceiver> cast_receiver(
      media::cast::CastReceiver::CreateCastReceiver(
      cast_environment,
      audio_config,
      video_config,
      transport->packet_sender()));

  media::cast::PacketReceiver* packet_receiver =
      cast_receiver->packet_receiver();

  media::cast::FrameReceiver* frame_receiver = cast_receiver->frame_receiver();

  int send_to_port, receive_port;
  media::cast::GetPorts(&send_to_port, &receive_port);
  std::string ip_address = media::cast::GetIpAddress();
  transport->SetLocalReceiver(packet_receiver, ip_address, receive_port);
  transport->SetSendDestination(ip_address, send_to_port);

  media::cast::ReceiveProcess receive_process(cast_environment, frame_receiver);
  receive_process.Start();
  main_message_loop.Run();
  transport->StopReceiving();
  return 0;
}
