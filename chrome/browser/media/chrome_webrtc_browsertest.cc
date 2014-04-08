// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/media/webrtc_browsertest_perf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/perf/perf_test.h"

// For fine-grained suppression.
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

// Top-level integration test for WebRTC. The test methods here must run
// sequentially since they use a server binary on the system (hence they are
// tagged as MANUAL). In addition, they need the reference videos which require
// the webrtc.DEPS solution, which is not generally available on Chromium bots.
class WebRtcBrowserTest : public WebRtcTestBase,
                          public testing::WithParamInterface<bool> {
 public:
  WebRtcBrowserTest() {}
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    test::PeerConnectionServerRunner::KillAllPeerConnectionServers();
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Play a suitable, somewhat realistic video file.
    base::FilePath input_video = test::GetReferenceFilesDir()
        .Append(test::kReferenceFileName360p)
        .AddExtension(test::kY4mFileExtension);
    command_line->AppendSwitchPath(switches::kUseFileForFakeVideoCapture,
                                   input_video);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // Flag used by TestWebAudioMediaStream to force garbage collection.
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");

    bool enable_audio_track_processing = GetParam();
    if (enable_audio_track_processing)
      command_line->AppendSwitch(switches::kEnableAudioTrackProcessing);
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

  // Tries to extract data from peerConnectionDataStore in the webrtc-internals
  // tab. The caller owns the parsed data. Returns NULL on failure.
  base::DictionaryValue* GetWebrtcInternalsData(
      content::WebContents* webrtc_internals_tab) {
    std::string all_stats_json = ExecuteJavascript(
        "window.domAutomationController.send("
        "    JSON.stringify(peerConnectionDataStore));",
        webrtc_internals_tab);

    base::Value* parsed_json = base::JSONReader::Read(all_stats_json);
    base::DictionaryValue* result;
    if (parsed_json && parsed_json->GetAsDictionary(&result))
      return result;

    return NULL;
  }

  const base::DictionaryValue* GetDataOnFirstPeerConnection(
      const base::DictionaryValue* all_data) {
    base::DictionaryValue::Iterator iterator(*all_data);

    const base::DictionaryValue* result;
    if (!iterator.IsAtEnd() && iterator.value().GetAsDictionary(&result))
      return result;

    return NULL;
  }

  bool OnWinXp() {
#if defined(OS_WIN)
    return base::win::GetVersion() <= base::win::VERSION_XP;
#else
    return false;
#endif
  }

  test::PeerConnectionServerRunner peerconnection_server_;
};

static const bool kRunTestsWithFlag[] = { false, true };
INSTANTIATE_TEST_CASE_P(WebRtcBrowserTests,
                        WebRtcBrowserTest,
                        testing::ValuesIn(kRunTestsWithFlag));

IN_PROC_BROWSER_TEST_P(WebRtcBrowserTest,
                       MANUAL_RunsAudioVideoWebRTCCallInTwoTabs) {
  if (OnWinXp()) return;

  ASSERT_TRUE(test::HasReferenceFilesInCheckout());
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(peerconnection_server_.Start());

  content::WebContents* left_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  content::WebContents* right_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

  EstablishCall(left_tab, right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideoToPlay(left_tab);
  WaitForVideoToPlay(right_tab);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  ASSERT_TRUE(peerconnection_server_.Stop());
}

IN_PROC_BROWSER_TEST_P(WebRtcBrowserTest, MANUAL_CpuUsage15Seconds) {
  if (OnWinXp()) return;

  ASSERT_TRUE(test::HasReferenceFilesInCheckout());
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(peerconnection_server_.Start());

  base::FilePath results_file;
  ASSERT_TRUE(base::CreateTemporaryFile(&results_file));

  content::WebContents* left_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

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

  content::WebContents* right_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

  EstablishCall(left_tab, right_tab);

  test::SleepInJavascript(left_tab, 15000);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

#if !defined(OS_MACOSX)
  PrintProcessMetrics(renderer_process_metrics.get(), "_r");
#endif
  PrintProcessMetrics(browser_process_metrics.get(), "_b");

  ASSERT_TRUE(peerconnection_server_.Stop());
}

// This is manual for its long execution time.
IN_PROC_BROWSER_TEST_P(WebRtcBrowserTest,
                       MANUAL_RunsAudioVideoCall60SecsAndLogsInternalMetrics) {
  if (OnWinXp()) return;

  ASSERT_TRUE(test::HasReferenceFilesInCheckout());
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(peerconnection_server_.Start());

  ASSERT_GE(TestTimeouts::action_max_timeout().InSeconds(), 100) <<
      "This is a long-running test; you must specify "
      "--ui-test-action-max-timeout to have a value of at least 100000.";

  content::WebContents* left_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  content::WebContents* right_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

  EstablishCall(left_tab, right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideoToPlay(left_tab);
  WaitForVideoToPlay(right_tab);

  // Let values stabilize, bandwidth ramp up, etc.
  test::SleepInJavascript(left_tab, 60000);

  // Start measurements.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://webrtc-internals"));
  content::WebContents* webrtc_internals_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  test::SleepInJavascript(left_tab, 10000);

  scoped_ptr<base::DictionaryValue> all_data(
      GetWebrtcInternalsData(webrtc_internals_tab));
  ASSERT_TRUE(all_data.get() != NULL);

  const base::DictionaryValue* first_pc_dict =
      GetDataOnFirstPeerConnection(all_data.get());
  ASSERT_TRUE(first_pc_dict != NULL);
  test::PrintBweForVideoMetrics(*first_pc_dict);
  test::PrintMetricsForAllStreams(*first_pc_dict);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  ASSERT_TRUE(peerconnection_server_.Stop());
}

IN_PROC_BROWSER_TEST_P(WebRtcBrowserTest, MANUAL_TestWebAudioMediaStream) {
  if (OnWinXp()) return;

  ASSERT_TRUE(test::HasReferenceFilesInCheckout());
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url(embedded_test_server()->GetURL("/webrtc/webaudio_crash.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // A sleep is necessary to be able to detect the crash.
  test::SleepInJavascript(tab, 1000);

  ASSERT_FALSE(tab->IsCrashed());
}
