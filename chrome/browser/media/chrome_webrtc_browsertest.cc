// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/perf/perf_test.h"

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

// Top-level integration test for WebRTC. Requires a real webcam and microphone
// on the running system. This test is not meant to run in the main browser
// test suite since normal tester machines do not have webcams.
class WebrtcBrowserTest : public WebRtcTestBase {
 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    PeerConnectionServerRunner::KillAllPeerConnectionServersOnCurrentSystem();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // This test expects real device handling and requires a real webcam / audio
    // device; it will not work with fake devices.
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeUIForMediaStream));

    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

  // Ensures we didn't get any errors asynchronously (e.g. while no javascript
  // call from this test was outstanding).
  void AssertNoAsynchronousErrors(content::WebContents* tab_contents) {
    EXPECT_EQ("ok-no-errors",
              ExecuteJavascript("getAnyTestFailures()", tab_contents));
  }

  void EstablishCall(content::WebContents* from_tab,
                     content::WebContents* to_tab) {
    ConnectToPeerConnectionServer("peer 1", from_tab);
    ConnectToPeerConnectionServer("peer 2", to_tab);

    EXPECT_EQ("ok-peerconnection-created",
              ExecuteJavascript("preparePeerConnection()", from_tab));
    EXPECT_EQ("ok-added",
              ExecuteJavascript("addLocalStream()", from_tab));
    EXPECT_EQ("ok-negotiating",
              ExecuteJavascript("negotiateCall()", from_tab));

    // Ensure the call gets up on both sides.
    EXPECT_TRUE(PollingWaitUntil("getPeerConnectionReadyState()",
                                 "active", from_tab));
    EXPECT_TRUE(PollingWaitUntil("getPeerConnectionReadyState()",
                                 "active", to_tab));

    AssertNoAsynchronousErrors(from_tab);
    AssertNoAsynchronousErrors(to_tab);
  }

  void StartDetectingVideo(content::WebContents* tab_contents,
                           const std::string& video_element) {
    std::string javascript = base::StringPrintf(
        "startDetection('%s', 'frame-buffer', 320, 240)",
        video_element.c_str());
    EXPECT_EQ("ok-started", ExecuteJavascript(javascript, tab_contents));
  }

  void WaitForVideoToPlay(content::WebContents* tab_contents) {
    EXPECT_TRUE(PollingWaitUntil("isVideoPlaying()", "video-playing",
                                 tab_contents));
  }

  void WaitForVideoToStopPlaying(content::WebContents* tab_contents) {
    EXPECT_TRUE(PollingWaitUntil("isVideoPlaying()", "video-not-playing",
                                 tab_contents));
  }

  void HangUp(content::WebContents* from_tab) {
    EXPECT_EQ("ok-call-hung-up", ExecuteJavascript("hangUp()", from_tab));
  }

  void WaitUntilHangupVerified(content::WebContents* tab_contents) {
    EXPECT_TRUE(PollingWaitUntil("getPeerConnectionReadyState()",
                                 "no-peer-connection", tab_contents));
  }

  std::string ToggleLocalVideoTrack(content::WebContents* tab_contents) {
    // Toggle the only video track in the page (e.g. video track 0).
    return ExecuteJavascript("toggleLocalStream("
        "function(local) { return local.getVideoTracks()[0]; }, "
        "'video');", tab_contents);
  }

  std::string ToggleRemoteVideoTrack(content::WebContents* tab_contents) {
    // Toggle the only video track in the page (e.g. video track 0).
    return ExecuteJavascript("toggleRemoteStream("
        "function(local) { return local.getVideoTracks()[0]; }, "
        "'video');", tab_contents);
  }

  void PrintProcessMetrics(base::ProcessMetrics* process_metrics,
                           const std::string& suffix) {
    perf_test::PrintResult("cpu", "", "cpu" + suffix,
                           process_metrics->GetCPUUsage(),
                           "%", true);
    perf_test::PrintResult("memory", "", "ws_peak" + suffix,
                           process_metrics->GetPeakWorkingSetSize(),
                           "bytes", true);
    perf_test::PrintResult("memory", "", "ws_final" + suffix,
                           process_metrics->GetWorkingSetSize(),
                           "bytes", false);

    size_t private_mem;
    size_t shared_mem;
    if (process_metrics->GetMemoryBytes(&private_mem, &shared_mem)) {
      perf_test::PrintResult("memory", "", "private_mem_final" + suffix,
                             private_mem, "bytes", false);
      perf_test::PrintResult("memory", "", "shared_mem_final" + suffix,
                             shared_mem, "bytes", false);
    }
  }

  std::string GetWebrtcInternalsData(
      content::WebContents* webrtc_internals_tab) {
    return ExecuteJavascript(
        "window.domAutomationController.send("
        "    JSON.stringify(peerConnectionDataStore));",
        webrtc_internals_tab);
  }

  void PrintInternalMetrics(const std::string& all_stats_json) {
    base::Value* parsed_json = base::JSONReader::Read(all_stats_json);
    ASSERT_TRUE(parsed_json != NULL) <<
        "Received bad JSON from webrtc-internals!";
    const base::DictionaryValue* json_dict;
    ASSERT_TRUE(parsed_json->GetAsDictionary(&json_dict));

    base::DictionaryValue::Iterator iterator(*json_dict);
    ASSERT_FALSE(iterator.IsAtEnd()) << "Didn't capture data about any peer "
         "connections in webrtc-internals.";

    const base::DictionaryValue* first_pc_dict;
    ASSERT_TRUE(iterator.value().GetAsDictionary(&first_pc_dict));

    std::string value;
    ASSERT_TRUE(first_pc_dict->GetString(
        "stats.bweforvideo-googAvailableSendBandwidth.values", &value));
    perf_test::PrintResult("bwe_stats", "", "available_send_bw", value, "bytes",
                           false);
    ASSERT_TRUE(first_pc_dict->GetString(
        "stats.bweforvideo-googAvailableReceiveBandwidth.values", &value));
    perf_test::PrintResult("bwe_stats", "", "available_recv_bw", value, "bytes",
                           false);
    ASSERT_TRUE(first_pc_dict->GetString(
        "stats.bweforvideo-googTargetEncBitrate.values", &value));
    perf_test::PrintResult("bwe_stats", "", "target_enc_bitrate",
                           value, "bytes", false);
    ASSERT_TRUE(first_pc_dict->GetString(
        "stats.bweforvideo-googActualEncBitrate.values", &value));
    perf_test::PrintResult("bwe_stats", "", "actual_enc_bitrate",
                           value, "bytes", false);
    ASSERT_TRUE(first_pc_dict->GetString(
        "stats.bweforvideo-googTransmitBitrate.values", &value));
    perf_test::PrintResult("bwe_stats", "", "transmit_bitrate", value, "bytes",
                           false);
  }

  content::WebContents* OpenTestPageAndGetUserMediaInNewTab() {
    chrome::AddBlankTabAt(browser(), -1, true);
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
    content::WebContents* left_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    GetUserMediaAndAccept(left_tab);
    return left_tab;
  }

  PeerConnectionServerRunner peerconnection_server_;
};

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MANUAL_RunsAudioVideoWebRTCCallInTwoTabs) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(peerconnection_server_.Start());

  content::WebContents* left_tab = OpenTestPageAndGetUserMediaInNewTab();
  content::WebContents* right_tab = OpenTestPageAndGetUserMediaInNewTab();

  EstablishCall(left_tab, right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideoToPlay(left_tab);
  WaitForVideoToPlay(right_tab);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  ASSERT_TRUE(peerconnection_server_.Stop());
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MANUAL_CpuUsage15Seconds) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(peerconnection_server_.Start());

  base::FilePath results_file;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&results_file));

  content::WebContents* left_tab = OpenTestPageAndGetUserMediaInNewTab();

#if defined(OS_MACOSX)
  // Don't measure renderer CPU on mac: requires a mach broker we don't have
  // access to from the browser test.
  scoped_ptr<base::ProcessMetrics> browser_process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(
      base::Process::Current().handle(), NULL));
  browser_process_metrics->GetCPUUsage();
#else
  // Measure rendering CPU on platforms that support it.
  base::ProcessHandle renderer_pid =
      left_tab->GetRenderProcessHost()->GetHandle();
  scoped_ptr<base::ProcessMetrics> renderer_process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(renderer_pid));
  renderer_process_metrics->GetCPUUsage();

  scoped_ptr<base::ProcessMetrics> browser_process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::Process::Current().handle()));
  browser_process_metrics->GetCPUUsage();
#endif

  content::WebContents* right_tab = OpenTestPageAndGetUserMediaInNewTab();

  EstablishCall(left_tab, right_tab);

  SleepInJavascript(left_tab, 15000);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

#if !defined(OS_MACOSX)
  PrintProcessMetrics(renderer_process_metrics.get(), "_r");
#endif
  PrintProcessMetrics(browser_process_metrics.get(), "_b");

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  ASSERT_TRUE(peerconnection_server_.Stop());
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MANUAL_TestMediaStreamTrackEnableDisable) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(peerconnection_server_.Start());

  content::WebContents* left_tab = OpenTestPageAndGetUserMediaInNewTab();
  content::WebContents* right_tab = OpenTestPageAndGetUserMediaInNewTab();

  EstablishCall(left_tab, right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideoToPlay(left_tab);
  WaitForVideoToPlay(right_tab);

  EXPECT_EQ("ok-video-toggled-to-false", ToggleLocalVideoTrack(left_tab));

  WaitForVideoToStopPlaying(right_tab);

  EXPECT_EQ("ok-video-toggled-to-true", ToggleLocalVideoTrack(left_tab));

  WaitForVideoToPlay(right_tab);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  ASSERT_TRUE(peerconnection_server_.Stop());
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MANUAL_RunsAudioVideoCall20SecsAndLogsInternalMetrics) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(peerconnection_server_.Start());

  content::WebContents* left_tab = OpenTestPageAndGetUserMediaInNewTab();
  content::WebContents* right_tab = OpenTestPageAndGetUserMediaInNewTab();

  EstablishCall(left_tab, right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideoToPlay(left_tab);
  WaitForVideoToPlay(right_tab);

  // Let values stabilize, bandwidth ramp up, etc.
  SleepInJavascript(left_tab, 10000);

  // Start measurements.
  chrome::AddBlankTabAt(browser(), -1, true);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://webrtc-internals"));
  content::WebContents* webrtc_internals_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  SleepInJavascript(left_tab, 10000);

  std::string all_stats_json = GetWebrtcInternalsData(webrtc_internals_tab);
  PrintInternalMetrics(all_stats_json);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  ASSERT_TRUE(peerconnection_server_.Stop());
}
