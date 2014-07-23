// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>

#include "base/command_line.h"
#include "base/float_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/switches.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/in_process_receiver.h"
#include "media/cast/test/utility/net_utility.h"
#include "media/cast/test/utility/standalone_cast_environment.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/rand_callback.h"
#include "net/udp/udp_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::cast::test::GetFreeLocalPort;

namespace extensions {

class CastStreamingApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
    command_line->AppendSwitchASCII(::switches::kWindowSize, "300,300");
  }
};

// Test running the test extension for Cast Mirroring API.
IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, Basics) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "basics.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, Stats) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "stats.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, BadLogging) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "bad_logging.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, DestinationNotSet) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "destination_not_set.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, StopNoStart) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "stop_no_start.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, NullStream) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "null_stream.html"))
      << message_;
}

namespace {

// An in-process Cast receiver that examines the audio/video frames being
// received for expected colors and tones.  Used in
// CastStreamingApiTest.EndToEnd, below.
class TestPatternReceiver : public media::cast::InProcessReceiver {
 public:
  explicit TestPatternReceiver(
      const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
      const net::IPEndPoint& local_end_point)
      : InProcessReceiver(cast_environment,
                          local_end_point,
                          net::IPEndPoint(),
                          media::cast::GetDefaultAudioReceiverConfig(),
                          media::cast::GetDefaultVideoReceiverConfig()),
        target_tone_frequency_(0),
        current_tone_frequency_(0.0f) {
    memset(&target_color_, 0, sizeof(target_color_));
    memset(&current_color_, 0, sizeof(current_color_));
  }

  virtual ~TestPatternReceiver() {}

  // Blocks the caller until this receiver has seen both |yuv_color| and
  // |tone_frequency| consistently for the given |duration|.
  void WaitForColorAndTone(const uint8 yuv_color[3],
                           int tone_frequency,
                           base::TimeDelta duration) {
    LOG(INFO) << "Waiting for test pattern: color=yuv("
              << static_cast<int>(yuv_color[0]) << ", "
              << static_cast<int>(yuv_color[1]) << ", "
              << static_cast<int>(yuv_color[2])
              << "), tone_frequency=" << tone_frequency << " Hz";

    base::RunLoop run_loop;
    cast_env()->PostTask(
        media::cast::CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&TestPatternReceiver::NotifyOnceMatched,
                   base::Unretained(this),
                   yuv_color,
                   tone_frequency,
                   duration,
                   media::BindToCurrentLoop(run_loop.QuitClosure())));
    run_loop.Run();
  }

 private:
  // Resets tracking data and sets the match duration and callback.
  void NotifyOnceMatched(const uint8 yuv_color[3],
                         int tone_frequency,
                         base::TimeDelta match_duration,
                         const base::Closure& matched_callback) {
    DCHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));

    match_duration_ = match_duration;
    matched_callback_ = matched_callback;
    target_color_[0] = yuv_color[0];
    target_color_[1] = yuv_color[1];
    target_color_[2] = yuv_color[2];
    target_tone_frequency_ = tone_frequency;
    first_time_near_target_color_ = base::TimeTicks();
    first_time_near_target_tone_ = base::TimeTicks();
  }

  // Runs |matched_callback_| once both color and tone have been matched for the
  // required |match_duration_|.
  void NotifyIfMatched() {
    DCHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));

    // TODO(miu): Check audio tone too, once audio is fixed in the library.
    // http://crbug.com/349295
    if (first_time_near_target_color_.is_null() ||
        /*first_time_near_target_tone_.is_null()*/ false)
      return;
    const base::TimeTicks now = cast_env()->Clock()->NowTicks();
    if ((now - first_time_near_target_color_) >= match_duration_ &&
        /*(now - first_time_near_target_tone_) >= match_duration_*/ true) {
      matched_callback_.Run();
    }
  }

  // Invoked by InProcessReceiver for each received audio frame.
  virtual void OnAudioFrame(scoped_ptr<media::AudioBus> audio_frame,
                            const base::TimeTicks& playout_time,
                            bool is_continuous) OVERRIDE {
    DCHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));

    if (audio_frame->frames() <= 0) {
      NOTREACHED() << "OnAudioFrame called with no samples?!?";
      return;
    }

    // Assume the audio signal is a single sine wave (it can have some
    // low-amplitude noise).  Count zero crossings, and extrapolate the
    // frequency of the sine wave in |audio_frame|.
    int crossings = 0;
    for (int ch = 0; ch < audio_frame->channels(); ++ch) {
      crossings += media::cast::CountZeroCrossings(audio_frame->channel(ch),
                                                   audio_frame->frames());
    }
    crossings /= audio_frame->channels();  // Take the average.
    const float seconds_per_frame =
        audio_frame->frames() / static_cast<float>(audio_config().frequency);
    const float frequency_in_frame = crossings / seconds_per_frame / 2.0f;

    const float kAveragingWeight = 0.1f;
    UpdateExponentialMovingAverage(
        kAveragingWeight, frequency_in_frame, &current_tone_frequency_);
    VLOG(1) << "Current audio tone frequency: " << current_tone_frequency_;

    const float kTargetWindowHz = 20;
    // Update the time at which the current tone started falling within
    // kTargetWindowHz of the target tone.
    if (fabsf(current_tone_frequency_ - target_tone_frequency_) <
        kTargetWindowHz) {
      if (first_time_near_target_tone_.is_null())
        first_time_near_target_tone_ = cast_env()->Clock()->NowTicks();
      NotifyIfMatched();
    } else {
      first_time_near_target_tone_ = base::TimeTicks();
    }
  }

  virtual void OnVideoFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                            const base::TimeTicks& render_time,
                            bool is_continuous) OVERRIDE {
    DCHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));

    CHECK(video_frame->format() == media::VideoFrame::YV12 ||
          video_frame->format() == media::VideoFrame::I420 ||
          video_frame->format() == media::VideoFrame::YV12A);

    // Note: We take the median value of each plane because the test image will
    // contain mostly a solid color plus some "cruft" which is the "Testing..."
    // text in the upper-left corner of the video frame.  In other words, we
    // want to read "the most common color."
    const int kPlanes[] = {media::VideoFrame::kYPlane,
                           media::VideoFrame::kUPlane,
                           media::VideoFrame::kVPlane};
    for (size_t i = 0; i < arraysize(kPlanes); ++i) {
      current_color_[i] =
          ComputeMedianIntensityInPlane(video_frame->row_bytes(kPlanes[i]),
                                        video_frame->rows(kPlanes[i]),
                                        video_frame->stride(kPlanes[i]),
                                        video_frame->data(kPlanes[i]));
    }

    VLOG(1) << "Current video color: yuv(" << current_color_[0] << ", "
            << current_color_[1] << ", " << current_color_[2] << ')';

    const float kTargetWindow = 10.0f;
    // Update the time at which all color channels started falling within
    // kTargetWindow of the target.
    if (fabsf(current_color_[0] - target_color_[0]) < kTargetWindow &&
        fabsf(current_color_[1] - target_color_[1]) < kTargetWindow &&
        fabsf(current_color_[2] - target_color_[2]) < kTargetWindow) {
      if (first_time_near_target_color_.is_null())
        first_time_near_target_color_ = cast_env()->Clock()->NowTicks();
      NotifyIfMatched();
    } else {
      first_time_near_target_color_ = base::TimeTicks();
    }
  }

  static void UpdateExponentialMovingAverage(float weight,
                                             float sample_value,
                                             float* average) {
    *average = weight * sample_value + (1.0f - weight) * (*average);
    CHECK(base::IsFinite(*average));
  }

  static uint8 ComputeMedianIntensityInPlane(int width,
                                             int height,
                                             int stride,
                                             uint8* data) {
    const int num_pixels = width * height;
    if (num_pixels <= 0)
      return 0;
    // If necessary, re-pack the pixels such that the stride is equal to the
    // width.
    if (width < stride) {
      for (int y = 1; y < height; ++y) {
        uint8* const src = data + y * stride;
        uint8* const dest = data + y * width;
        memmove(dest, src, width);
      }
    }
    const size_t middle_idx = num_pixels / 2;
    std::nth_element(data, data + middle_idx, data + num_pixels);
    return data[middle_idx];
  }

  base::TimeDelta match_duration_;
  base::Closure matched_callback_;

  float target_color_[3];  // Y, U, V
  float target_tone_frequency_;

  float current_color_[3];  // Y, U, V
  base::TimeTicks first_time_near_target_color_;
  float current_tone_frequency_;
  base::TimeTicks first_time_near_target_tone_;

  DISALLOW_COPY_AND_ASSIGN(TestPatternReceiver);
};

}  // namespace

class CastStreamingApiTestWithPixelOutput : public CastStreamingApiTest {
  virtual void SetUp() OVERRIDE {
    EnablePixelOutput();
    CastStreamingApiTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(::switches::kWindowSize, "128,128");
    CastStreamingApiTest::SetUpCommandLine(command_line);
  }
};

// http://crbug.com/396413
// Tests the Cast streaming API and its basic functionality end-to-end.  An
// extension subtest is run to generate test content, capture that content, and
// use the API to send it out.  At the same time, this test launches an
// in-process Cast receiver, listening on a localhost UDP socket, to receive the
// content and check whether it matches expectations.
IN_PROC_BROWSER_TEST_F(CastStreamingApiTestWithPixelOutput, DISABLED_EndToEnd) {
  scoped_ptr<net::UDPSocket> receive_socket(
      new net::UDPSocket(net::DatagramSocket::DEFAULT_BIND,
                         net::RandIntCallback(),
                         NULL,
                         net::NetLog::Source()));
  receive_socket->AllowAddressReuse();
  ASSERT_EQ(net::OK, receive_socket->Bind(GetFreeLocalPort()));
  net::IPEndPoint receiver_end_point;
  ASSERT_EQ(net::OK, receive_socket->GetLocalAddress(&receiver_end_point));
  receive_socket.reset();

  // Start the in-process receiver that examines audio/video for the expected
  // test patterns.
  const scoped_refptr<media::cast::StandaloneCastEnvironment> cast_environment(
      new media::cast::StandaloneCastEnvironment());
  TestPatternReceiver* const receiver =
      new TestPatternReceiver(cast_environment, receiver_end_point);

  // Launch the page that: 1) renders the source content; 2) uses the
  // chrome.tabCapture and chrome.cast.streaming APIs to capture its content and
  // stream using Cast; and 3) calls chrome.test.succeed() once it is
  // operational.
  const std::string page_url = base::StringPrintf(
      "end_to_end_sender.html?port=%d", receiver_end_point.port());
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", page_url)) << message_;

  // Examine the Cast receiver for expected audio/video test patterns.  The
  // colors and tones specified here must match those in end_to_end_sender.js.
  receiver->Start();
  const uint8 kRedInYUV[3] = {82, 90, 240};    // rgb(255, 0, 0)
  const uint8 kGreenInYUV[3] = {145, 54, 34};  // rgb(0, 255, 0)
  const uint8 kBlueInYUV[3] = {41, 240, 110};  // rgb(0, 0, 255)
  const base::TimeDelta kOneHalfSecond = base::TimeDelta::FromMilliseconds(500);
  receiver->WaitForColorAndTone(kRedInYUV, 200 /* Hz */, kOneHalfSecond);
  receiver->WaitForColorAndTone(kGreenInYUV, 500 /* Hz */, kOneHalfSecond);
  receiver->WaitForColorAndTone(kBlueInYUV, 1800 /* Hz */, kOneHalfSecond);
  receiver->Stop();

  delete receiver;
  cast_environment->Shutdown();
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTestWithPixelOutput, RtpStreamError) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "rtp_stream_error.html"));
}

}  // namespace extensions
