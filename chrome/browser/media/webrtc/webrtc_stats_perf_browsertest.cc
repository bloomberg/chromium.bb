// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/test_stats_dictionary.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"
#include "media/base/media_switches.h"
#include "testing/perf/perf_test.h"

namespace content {

namespace {

const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";

const char kInboundRtp[] = "inbound-rtp";
const char kOutboundRtp[] = "outbound-rtp";

// Sums up "RTC[In/Out]boundRTPStreamStats.bytes_[received/sent]" values.
double GetTotalRTPStreamBytes(
    TestStatsReportDictionary* report, const char* type,
    const char* media_type) {
  DCHECK(type == kInboundRtp || type == kOutboundRtp);
  const char* bytes_name =
      (type == kInboundRtp) ? "bytesReceived" : "bytesSent";
  double total_bytes = 0.0;
  report->ForEach([&type, &bytes_name, &media_type, &total_bytes](
      const TestStatsDictionary& stats) {
    if (stats.GetString("type") == type &&
        stats.GetString("mediaType") == media_type) {
      total_bytes += stats.GetNumber(bytes_name);
    }
  });
  return total_bytes;
}

double GetAudioBytesSent(TestStatsReportDictionary* report) {
  return GetTotalRTPStreamBytes(report, kOutboundRtp, "audio");
}

double GetAudioBytesReceived(TestStatsReportDictionary* report) {
  return GetTotalRTPStreamBytes(report, kInboundRtp, "audio");
}

double GetVideoBytesSent(TestStatsReportDictionary* report) {
  return GetTotalRTPStreamBytes(report, kOutboundRtp, "video");
}

double GetVideoBytesReceived(TestStatsReportDictionary* report) {
  return GetTotalRTPStreamBytes(report, kInboundRtp, "video");
}

// Performance browsertest for WebRTC. This test is manual since it takes long
// to execute and requires the reference files provided by the webrtc.DEPS
// solution (which is only available on WebRTC internal bots).
// Gets its metrics from the standards conformant "RTCPeerConnection.getStats".
class WebRtcStatsPerfBrowserTest : public WebRtcTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Play a suitable, somewhat realistic video file.
    base::FilePath input_video = test::GetReferenceFilesDir()
        .Append(test::kReferenceFileName360p)
        .AddExtension(test::kY4mFileExtension);
    command_line->AppendSwitchPath(switches::kUseFileForFakeVideoCapture,
                                   input_video);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "RTCPeerConnectionNewGetStats");
  }

  void RunsAudioAndVideoCall(
      const std::string& audio_codec, const std::string& video_codec) {
    ASSERT_TRUE(test::HasReferenceFilesInCheckout());
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(audio_codec != kUseDefaultAudioCodec ||
                video_codec != kUseDefaultVideoCodec);

    ASSERT_GE(TestTimeouts::action_max_timeout().InSeconds(), 100)
        << "This is a long-running test; you must specify "
           "--ui-test-action-max-timeout to have a value of at least 100000.";

    content::WebContents* left_tab =
        OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
    content::WebContents* right_tab =
        OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

    SetupPeerconnectionWithLocalStream(left_tab);
    SetupPeerconnectionWithLocalStream(right_tab);
    SetDefaultAudioCodec(left_tab, audio_codec);
    SetDefaultAudioCodec(right_tab, audio_codec);
    SetDefaultVideoCodec(left_tab, video_codec);
    SetDefaultVideoCodec(right_tab, video_codec);
    NegotiateCall(left_tab, right_tab);
    StartDetectingVideo(left_tab, "remote-view");
    StartDetectingVideo(right_tab, "remote-view");
    WaitForVideoToPlay(left_tab);
    WaitForVideoToPlay(right_tab);

    // Call for 60 seconds so that values may stabilize, bandwidth ramp up, etc.
    test::SleepInJavascript(left_tab, 60000);

    // The ramp-up may vary greatly and impact the resulting total bytes, to get
    // reliable measurements we do two measurements, at 60 and 70 seconds and
    // look at the average bytes/second in that window.
    double audio_bytes_sent_before = 0.0;
    double audio_bytes_received_before = 0.0;
    double video_bytes_sent_before = 0.0;
    double video_bytes_received_before = 0.0;

    scoped_refptr<TestStatsReportDictionary> report =
        GetStatsReportDictionary(left_tab);
    if (audio_codec != kUseDefaultAudioCodec) {
      audio_bytes_sent_before = GetAudioBytesSent(report.get());
      audio_bytes_received_before = GetAudioBytesReceived(report.get());

    }
    if (video_codec != kUseDefaultVideoCodec) {
      video_bytes_sent_before = GetVideoBytesSent(report.get());
      video_bytes_received_before = GetVideoBytesReceived(report.get());
    }

    double measure_duration_seconds = 10.0;
    test::SleepInJavascript(left_tab, static_cast<int>(
        measure_duration_seconds * base::Time::kMillisecondsPerSecond));

    report = GetStatsReportDictionary(left_tab);
    if (audio_codec != kUseDefaultAudioCodec) {
      double audio_bytes_sent_after = GetAudioBytesSent(report.get());
      double audio_bytes_received_after = GetAudioBytesReceived(report.get());

      double audio_send_rate =
          (audio_bytes_sent_after - audio_bytes_sent_before) /
          measure_duration_seconds;
      double audio_receive_rate =
          (audio_bytes_received_after - audio_bytes_received_before) /
          measure_duration_seconds;

      std::string audio_codec_modifier = "_" + audio_codec;
      perf_test::PrintResult(
          "audio", audio_codec_modifier, "send_rate", audio_send_rate,
          "bytes/second", false);
      perf_test::PrintResult(
          "audio", audio_codec_modifier, "receive_rate", audio_receive_rate,
          "bytes/second", false);
    }
    if (video_codec != kUseDefaultVideoCodec) {
      double video_bytes_sent_after = GetVideoBytesSent(report.get());
      double video_bytes_received_after = GetVideoBytesReceived(report.get());

      double video_send_rate =
          (video_bytes_sent_after - video_bytes_sent_before) /
          measure_duration_seconds;
      double video_receive_rate =
          (video_bytes_received_after - video_bytes_received_before) /
          measure_duration_seconds;

      std::string video_codec_modifier = "_" + video_codec;
      perf_test::PrintResult(
          "video", video_codec_modifier, "send_rate", video_send_rate,
          "bytes/second", false);
      perf_test::PrintResult(
          "video", video_codec_modifier, "receive_rate", video_receive_rate,
          "bytes/second", false);
    }

    HangUp(left_tab);
    HangUp(right_tab);
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcStatsPerfBrowserTest,
                       MANUAL_RunsAudioAndVideoCall_AudioCodec_opus) {
  RunsAudioAndVideoCall("opus", kUseDefaultVideoCodec);
}

IN_PROC_BROWSER_TEST_F(WebRtcStatsPerfBrowserTest,
                       MANUAL_RunsAudioAndVideoCall_AudioCodec_ISAC) {
  RunsAudioAndVideoCall("ISAC", kUseDefaultVideoCodec);
}

IN_PROC_BROWSER_TEST_F(WebRtcStatsPerfBrowserTest,
                       MANUAL_RunsAudioAndVideoCall_AudioCodec_G722) {
  RunsAudioAndVideoCall("G722", kUseDefaultVideoCodec);
}

IN_PROC_BROWSER_TEST_F(WebRtcStatsPerfBrowserTest,
                       MANUAL_RunsAudioAndVideoCall_AudioCodec_PCMU) {
  RunsAudioAndVideoCall("PCMU", kUseDefaultVideoCodec);
}

IN_PROC_BROWSER_TEST_F(WebRtcStatsPerfBrowserTest,
                       MANUAL_RunsAudioAndVideoCall_AudioCodec_PCMA) {
  RunsAudioAndVideoCall("PCMA", kUseDefaultVideoCodec);
}

IN_PROC_BROWSER_TEST_F(WebRtcStatsPerfBrowserTest,
                       MANUAL_RunsAudioAndVideoCall_VideoCodec_VP8) {
  RunsAudioAndVideoCall(kUseDefaultAudioCodec, "VP8");
}

IN_PROC_BROWSER_TEST_F(WebRtcStatsPerfBrowserTest,
                       MANUAL_RunsAudioAndVideoCall_VideoCodec_VP9) {
  RunsAudioAndVideoCall(kUseDefaultAudioCodec, "VP9");
}

#if BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(WebRtcStatsPerfBrowserTest,
                       MANUAL_RunsAudioAndVideoCall_VideoCodec_H264) {
  // Only run test if run-time feature corresponding to |rtc_use_h264| is on.
  if (!base::FeatureList::IsEnabled(content::kWebRtcH264WithOpenH264FFmpeg)) {
    LOG(WARNING) << "Run-time feature WebRTC-H264WithOpenH264FFmpeg disabled. "
        "Skipping WebRtcPerfBrowserTest."
        "MANUAL_RunsAudioAndVideoCall_VideoCodec_H264 (test \"OK\")";
    return;
  }
  RunsAudioAndVideoCall(kUseDefaultAudioCodec, "H264");
}

#endif  // BUILDFLAG(RTC_USE_H264)

}  // namespace

}  // namespace content
