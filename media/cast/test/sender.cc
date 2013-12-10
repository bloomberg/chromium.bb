// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test application that simulates a cast sender - Data can be either generated
// or read from a file.

#include "base/at_exit.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/test/audio_utility.h"
#include "media/cast/test/transport/transport.h"
#include "media/cast/test/utility/input_helper.h"
#include "media/cast/test/video_utility.h"
#include "ui/gfx/size.h"

namespace media {
namespace cast {
// Settings chosen to match default receiver settings.
#define DEFAULT_SEND_PORT "2344"
#define DEFAULT_RECEIVE_PORT "2346"
#define DEFAULT_SEND_IP "127.0.0.1"
#define DEFAULT_READ_FROM_FILE "0"
#define DEFAULT_PACKET_LOSS "0"
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
void OwnThatAudioBus(scoped_ptr<AudioBus> audio_bus) {
}
} // namespace

void GetPorts(int* tx_port, int* rx_port) {
  test::InputBuilder tx_input("Enter send port.",
      DEFAULT_SEND_PORT, 1, INT_MAX);
  *tx_port = tx_input.GetIntInput();

  test::InputBuilder rx_input("Enter receive port.",
      DEFAULT_RECEIVE_PORT, 1, INT_MAX);
  *rx_port = rx_input.GetIntInput();
}

int GetPacketLoss() {
  test::InputBuilder input("Enter send side packet loss %.",
      DEFAULT_PACKET_LOSS, 0, 99);
  return input.GetIntInput();
}

std::string GetIpAddress(const std::string display_text) {
  test::InputBuilder input(display_text, DEFAULT_SEND_IP,
      INT_MIN, INT_MAX);
  std::string ip_address = input.GetStringInput();
  // Verify correct form:
  while (std::count(ip_address.begin(), ip_address.end(), '.') != 3) {
    ip_address = input.GetStringInput();
  }
  return ip_address;
}

bool ReadFromFile() {
  test::InputBuilder input("Enter 1 to read from file.", DEFAULT_READ_FROM_FILE,
      0, 1);
  return (1 == input.GetIntInput());
}

std::string GetVideoFile() {
  test::InputBuilder input("Enter file and path to raw video file.","",
      INT_MIN, INT_MAX);
  return input.GetStringInput();
}

void GetSsrcs(AudioSenderConfig* audio_config) {
  test::InputBuilder input_tx("Choose audio sender SSRC.",
      DEFAULT_AUDIO_SENDER_SSRC, 1, INT_MAX);
  audio_config->sender_ssrc = input_tx.GetIntInput();

  test::InputBuilder input_rx("Choose audio receiver SSRC.",
      DEFAULT_AUDIO_RECEIVER_SSRC, 1, INT_MAX);
  audio_config->incoming_feedback_ssrc = input_rx.GetIntInput();
}

void GetSsrcs(VideoSenderConfig* video_config) {
  test::InputBuilder input_tx("Choose video sender SSRC.",
      DEFAULT_VIDEO_SENDER_SSRC, 1, INT_MAX);
  video_config->sender_ssrc = input_tx.GetIntInput();

  test::InputBuilder input_rx("Choose video receiver SSRC.",
      DEFAULT_VIDEO_RECEIVER_SSRC, 1, INT_MAX);
  video_config->incoming_feedback_ssrc = input_rx.GetIntInput();
}

void GetPayloadtype(AudioSenderConfig* audio_config) {
  test::InputBuilder input("Choose audio sender payload type.",
      DEFAULT_AUDIO_PAYLOAD_TYPE, 96, 127);
  audio_config->rtp_payload_type = input.GetIntInput();
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
  audio_config.codec = kOpus;
  return audio_config;
}

void GetPayloadtype(VideoSenderConfig* video_config) {
  test::InputBuilder input("Choose video sender payload type.",
      DEFAULT_VIDEO_PAYLOAD_TYPE, 96, 127);
  video_config->rtp_payload_type = input.GetIntInput();
}

void GetVideoCodecSize(VideoSenderConfig* video_config) {
  test::InputBuilder input_width("Choose video width.",
      DEFAULT_VIDEO_CODEC_WIDTH, 144, 1920);
  video_config->width = input_width.GetIntInput();

  test::InputBuilder input_height("Choose video height.",
      DEFAULT_VIDEO_CODEC_HEIGHT, 176, 1080);
  video_config->height = input_height.GetIntInput();
}

void GetVideoBitrates(VideoSenderConfig* video_config) {
  test::InputBuilder input_start_br("Choose start bitrate[kbps].",
      DEFAULT_VIDEO_CODEC_BITRATE, 0, INT_MAX);
  video_config->start_bitrate = input_start_br.GetIntInput() * 1000;

  test::InputBuilder input_max_br("Choose max bitrate[kbps].",
      DEFAULT_VIDEO_CODEC_MAX_BITRATE, 0, INT_MAX);
  video_config->max_bitrate = input_max_br.GetIntInput() * 1000;

  test::InputBuilder input_min_br("Choose min bitrate[kbps].",
      DEFAULT_VIDEO_CODEC_MIN_BITRATE, 0, INT_MAX);
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
  video_config.codec = kVp8;
  video_config.max_number_of_video_buffers_used = 1;
  video_config.number_of_cores = 1;
  return video_config;
}

class SendProcess {
 public:
  SendProcess(scoped_refptr<base::TaskRunner> thread_proxy,
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
        kAudioSamplingFrequency, kSoundFrequency, kSoundVolume));
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
    frame_input_->InsertAudio(audio_bus_ptr, clock_->NowTicks(),
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
      VLOG(1) << "Wait" <<
          (video_frame_time - elapsed_time).InMilliseconds();
     test_app_thread_proxy_->PostDelayedTask(FROM_HERE,
        base::Bind(&SendProcess::SendVideoFrameOnTime, base::Unretained(this),
                   video_frame),
        video_frame_time - elapsed_time);
    } else {
      test_app_thread_proxy_->PostTask(FROM_HERE,
      base::Bind(&SendProcess::SendVideoFrameOnTime, base::Unretained(this),
                 video_frame));
    }
  }

  void SendVideoFrameOnTime(scoped_refptr<media::VideoFrame> video_frame) {
    send_time_ = clock_->NowTicks();
    frame_input_->InsertRawVideoFrame(video_frame, send_time_);
    test_app_thread_proxy_->PostTask(FROM_HERE,
          base::Bind(&SendProcess::SendFrame, base::Unretained(this)));
  }

 private:
  scoped_refptr<base::TaskRunner> test_app_thread_proxy_;
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


int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  VLOG(1) << "Cast Sender";
  base::Thread test_thread("Cast sender test app thread");
  base::Thread main_thread("Cast main send thread");
  base::Thread audio_thread("Cast audio encoder thread");
  base::Thread video_thread("Cast video encoder thread");
  test_thread.Start();
  main_thread.Start();
  audio_thread.Start();
  video_thread.Start();

  base::DefaultTickClock clock;
  base::MessageLoopForIO io_message_loop;

  // Enable main and send side threads only. Disable logging.
  scoped_refptr<media::cast::CastEnvironment> cast_environment(new
      media::cast::CastEnvironment(
      &clock,
      main_thread.message_loop_proxy(),
      audio_thread.message_loop_proxy(),
      NULL,
      video_thread.message_loop_proxy(),
      NULL,
      media::cast::GetDefaultCastLoggingConfig()));

  media::cast::AudioSenderConfig audio_config =
      media::cast::GetAudioSenderConfig();
  media::cast::VideoSenderConfig video_config =
      media::cast::GetVideoSenderConfig();

  scoped_ptr<media::cast::test::Transport> transport(
      new media::cast::test::Transport(io_message_loop.message_loop_proxy()));
  scoped_ptr<media::cast::CastSender> cast_sender(
      media::cast::CastSender::CreateCastSender(cast_environment,
      audio_config,
      video_config,
      NULL,  // VideoEncoderController.
      transport->packet_sender()));

  media::cast::PacketReceiver* packet_receiver = cast_sender->packet_receiver();

  int send_to_port, receive_port;
  media::cast::GetPorts(&send_to_port, &receive_port);
  std::string ip_address = media::cast::GetIpAddress("Enter destination IP.");
  std::string local_ip_address = media::cast::GetIpAddress("Enter local IP.");
  int packet_loss_percentage = media::cast::GetPacketLoss();

  transport->SetLocalReceiver(packet_receiver, ip_address, local_ip_address,
                              receive_port);
  transport->SetSendDestination(ip_address, send_to_port);
  transport->SetSendSidePacketLoss(packet_loss_percentage);

  media::cast::FrameInput* frame_input = cast_sender->frame_input();
  scoped_ptr<media::cast::SendProcess> send_process(new
      media::cast::SendProcess(test_thread.message_loop_proxy(),
                               cast_environment->Clock(),
                               video_config,
                               frame_input));

  send_process->SendFrame();
  io_message_loop.Run();
  transport->StopReceiving();
  return 0;
}
