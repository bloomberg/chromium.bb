// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/media/webrtc_log_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/perf/perf_test.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/python_utils.h"

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

static const char kTestLoggingSessionId[] = "0123456789abcdef";

// Top-level integration test for WebRTC. Requires a real webcam and microphone
// on the running system. This test is not meant to run in the main browser
// test suite since normal tester machines do not have webcams.
class WebrtcBrowserTest : public WebRtcTestBase {
 public:
  // See comment in test where this class is used for purpose.
  class BrowserMessageFilter : public content::BrowserMessageFilter {
   public:
    explicit BrowserMessageFilter(
        const base::Closure& on_channel_closing)
        : on_channel_closing_(on_channel_closing) {}

    virtual void OnChannelClosing() OVERRIDE {
      // Posting on the file thread ensures that the callback is run after
      // WebRtcLogUploader::UploadLog has finished. See also comment in
      // MANUAL_RunsAudioVideoWebRTCCallInTwoTabsWithLogging test.
      content::BrowserThread::PostTask(content::BrowserThread::FILE,
          FROM_HERE, on_channel_closing_);
    }

    virtual bool OnMessageReceived(const IPC::Message& message,
                                   bool* message_was_ok) OVERRIDE {
      return false;
    }

  private:
    virtual ~BrowserMessageFilter() {}

    base::Closure on_channel_closing_;
  };

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    PeerConnectionServerRunner::KillAllPeerConnectionServersOnCurrentSystem();
    peerconnection_server_.Start();
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    peerconnection_server_.Stop();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // TODO(phoglund): check that user actually has the requisite devices and
    // print a nice message if not; otherwise the test just times out which can
    // be confusing.
    // This test expects real device handling and requires a real webcam / audio
    // device; it will not work with fake devices.
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeUIForMediaStream));

    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

  // Convenience method which executes the provided javascript in the context
  // of the provided web contents and returns what it evaluated to.
  std::string ExecuteJavascript(const std::string& javascript,
                                content::WebContents* tab_contents) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        tab_contents, javascript, &result));
    return result;
  }

  // Ensures we didn't get any errors asynchronously (e.g. while no javascript
  // call from this test was outstanding).
  // TODO(phoglund): this becomes obsolete when we switch to communicating with
  // the DOM message queue.
  void AssertNoAsynchronousErrors(content::WebContents* tab_contents) {
    EXPECT_EQ("ok-no-errors",
              ExecuteJavascript("getAnyTestFailures()", tab_contents));
  }

  // The peer connection server lets our two tabs find each other and talk to
  // each other (e.g. it is the application-specific "signaling solution").
  void ConnectToPeerConnectionServer(const std::string peer_name,
                                     content::WebContents* tab_contents) {
    std::string javascript = base::StringPrintf(
        "connect('http://localhost:8888', '%s');", peer_name.c_str());
    EXPECT_EQ("ok-connected", ExecuteJavascript(javascript, tab_contents));
  }

  void EstablishCall(content::WebContents* from_tab,
                     content::WebContents* to_tab,
                     bool enable_logging = false) {
    std::string javascript = enable_logging ?
        base::StringPrintf("preparePeerConnection(\"%s\")",
            kTestLoggingSessionId) :
        "preparePeerConnection(false)";
    EXPECT_EQ("ok-peerconnection-created",
              ExecuteJavascript(javascript, from_tab));
    EXPECT_EQ("ok-added",
              ExecuteJavascript("addLocalStream()", from_tab));
    EXPECT_EQ("ok-negotiating",
              ExecuteJavascript("negotiateCall()", from_tab));

    // Ensure the call gets up on both sides.
    EXPECT_TRUE(PollingWaitUntil("getPeerConnectionReadyState()",
                                 "active", from_tab));
    EXPECT_TRUE(PollingWaitUntil("getPeerConnectionReadyState()",
                                 "active", to_tab));
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
    perf_test::PrintResult("memory", "", "ws_final" + suffix,
                           process_metrics->GetWorkingSetSize(),
                           "bytes", true);
    perf_test::PrintResult("memory", "", "ws_peak" + suffix,
                           process_metrics->GetPeakWorkingSetSize(),
                           "bytes", true);

    size_t private_mem;
    size_t shared_mem;
    if (process_metrics->GetMemoryBytes(&private_mem, &shared_mem)) {
      perf_test::PrintResult("memory", "", "private_mem_final" + suffix,
                             private_mem, "bytes", true);
      perf_test::PrintResult("memory", "", "shared_mem_final" + suffix,
                             shared_mem, "bytes", true);
    }
  }

 private:
  PeerConnectionServerRunner peerconnection_server_;
};

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MANUAL_RunsAudioVideoWebRTCCallInTwoTabs) {
  EXPECT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  content::WebContents* left_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(left_tab);

  chrome::AddBlankTabAt(browser(), -1, true);
  content::WebContents* right_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  GetUserMediaAndAccept(right_tab);

  ConnectToPeerConnectionServer("peer 1", left_tab);
  ConnectToPeerConnectionServer("peer 2", right_tab);

  EstablishCall(left_tab, right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideoToPlay(left_tab);
  WaitForVideoToPlay(right_tab);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);
}

// TODO(phoglund): figure out how to do mach port brokering on Mac.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MANUAL_RendererCpuUsage20Seconds) {
  EXPECT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  base::FilePath results_file;
  EXPECT_TRUE(file_util::CreateTemporaryFile(&results_file));

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  content::WebContents* left_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::ProcessHandle renderer_pid =
      left_tab->GetRenderProcessHost()->GetHandle();

  scoped_ptr<base::ProcessMetrics> renderer_process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(renderer_pid));
  scoped_ptr<base::ProcessMetrics> browser_process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::Process::Current().handle()));

  // Start measuring CPU.
  renderer_process_metrics->GetCPUUsage();
  browser_process_metrics->GetCPUUsage();

  GetUserMediaAndAccept(left_tab);

  chrome::AddBlankTabAt(browser(), -1, true);
  content::WebContents* right_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  GetUserMediaAndAccept(right_tab);

  ConnectToPeerConnectionServer("peer 1", left_tab);
  ConnectToPeerConnectionServer("peer 2", right_tab);

  EstablishCall(left_tab, right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  SleepInJavascript(left_tab, 15000);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  PrintProcessMetrics(renderer_process_metrics.get(), "_r");
  PrintProcessMetrics(browser_process_metrics.get(), "_b");

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);
}
#endif

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MANUAL_TestMediaStreamTrackEnableDisable) {
  EXPECT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  content::WebContents* left_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(left_tab);

  chrome::AddBlankTabAt(browser(), -1, true);
  content::WebContents* right_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  GetUserMediaAndAccept(right_tab);

  ConnectToPeerConnectionServer("peer 1", left_tab);
  ConnectToPeerConnectionServer("peer 2", right_tab);

  EstablishCall(left_tab, right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

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
}

// Tests WebRTC diagnostic logging. Sets up the browser to save the multipart
// contents to a buffer instead of uploading it, then verifies it after a call.
// Example of multipart contents:
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="prod"
//
// Chrome_Linux
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="ver"
//
// 30.0.1554.0
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="guid"
//
// 0
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="type"
//
// webrtc_log
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="app_session_id"
//
// 0123456789abcdef
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="url"
//
// http://127.0.0.1:43213/webrtc/webrtc_jsep01_test.html
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="webrtc_log"; filename="webrtc_log.gz"
// Content-Type: application/gzip
//
// <compressed data (zip)>
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**------
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MANUAL_RunsAudioVideoWebRTCCallInTwoTabsWithLogging) {
  EXPECT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Add command line switch that forces allowing log uploads.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kEnableMetricsReportingForTesting);

  // Tell the uploader to save the multipart to a buffer instead of uploading.
  std::string multipart;
  g_browser_process->webrtc_log_uploader()->
      OverrideUploadWithBufferForTesting(&multipart);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  content::WebContents* left_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(left_tab);

  chrome::AddBlankTabAt(browser(), -1, true);
  content::WebContents* right_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  GetUserMediaAndAccept(right_tab);

  ConnectToPeerConnectionServer("peer 1", left_tab);
  ConnectToPeerConnectionServer("peer 2", right_tab);

  EstablishCall(left_tab, right_tab, true);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideoToPlay(left_tab);
  WaitForVideoToPlay(right_tab);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  // Uploading (or storing to our buffer in our test case) is triggered when
  // the tab is closed. We must ensure that WebRtcLogUploader::UploadLog, which
  // runs on the FILE thread, has finished after closing the tab before
  // continuing.
  // 1. Add a filter which just acts as a listener. It's added after
  //    WebRtcLoggingHandlerHost, which is the filter that posts a task for
  //    WebRtcLogUploader::UploadLog. So it's guarantueed to be removed after
  //    WebRtcLoggingHandlerHost.
  // 2. When the filter goes away post a task on the file thread to signal the
  //    event.
  base::WaitableEvent ipc_channel_closed(false, false);
  left_tab->GetRenderProcessHost()->GetChannel()->AddFilter(
      new BrowserMessageFilter(base::Bind(
          &base::WaitableEvent::Signal,
          base::Unretained(&ipc_channel_closed))));
  chrome::CloseWebContents(browser(), left_tab, false);
  ASSERT_TRUE(ipc_channel_closed.TimedWait(TestTimeouts::action_timeout()));

  const char boundary[] = "------**--yradnuoBgoLtrapitluMklaTelgooG--**----";

  // Remove the compressed data, it may contain "\r\n". Just verify that its
  // size is > 0.
  const char zip_content_type[] = "Content-Type: application/gzip";
  size_t zip_pos = multipart.find(&zip_content_type[0]);
  ASSERT_NE(std::string::npos, zip_pos);
  // Move pos to where the zip begins. - 1 to remove '\0', + 4 for two "\r\n".
  zip_pos += sizeof(zip_content_type) + 3;
  size_t zip_length = multipart.find(boundary, zip_pos);
  ASSERT_NE(std::string::npos, zip_length);
  // Calculate length, adjust for a "\r\n".
  zip_length -= zip_pos + 2;
  ASSERT_GT(zip_length, 0u);
  multipart.erase(zip_pos, zip_length);

  // Check the multipart contents.
  std::vector<std::string> multipart_lines;
  base::SplitStringUsingSubstr(multipart, "\r\n", &multipart_lines);
  ASSERT_EQ(31, static_cast<int>(multipart_lines.size()));

  EXPECT_STREQ(&boundary[0], multipart_lines[0].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"prod\"",
               multipart_lines[1].c_str());
  EXPECT_TRUE(multipart_lines[2].empty());
  EXPECT_NE(std::string::npos, multipart_lines[3].find("Chrome"));

  EXPECT_STREQ(&boundary[0], multipart_lines[4].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"ver\"",
               multipart_lines[5].c_str());
  EXPECT_TRUE(multipart_lines[6].empty());
  // Just check that the version contains a dot.
  EXPECT_NE(std::string::npos, multipart_lines[7].find('.'));

  EXPECT_STREQ(&boundary[0], multipart_lines[8].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"guid\"",
               multipart_lines[9].c_str());
  EXPECT_TRUE(multipart_lines[10].empty());
  EXPECT_STREQ("0", multipart_lines[11].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[12].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"type\"",
               multipart_lines[13].c_str());
  EXPECT_TRUE(multipart_lines[14].empty());
  EXPECT_STREQ("webrtc_log", multipart_lines[15].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[16].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"app_session_id\"",
               multipart_lines[17].c_str());
  EXPECT_TRUE(multipart_lines[18].empty());
  EXPECT_STREQ(kTestLoggingSessionId, multipart_lines[19].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[20].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"url\"",
               multipart_lines[21].c_str());
  EXPECT_TRUE(multipart_lines[22].empty());
  EXPECT_NE(std::string::npos,
            multipart_lines[23].find(&kMainWebrtcTestHtmlPage[0]));

  EXPECT_STREQ(&boundary[0], multipart_lines[24].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"webrtc_log\";"
               " filename=\"webrtc_log.gz\"",
               multipart_lines[25].c_str());
  EXPECT_STREQ("Content-Type: application/gzip",
               multipart_lines[26].c_str());
  EXPECT_TRUE(multipart_lines[27].empty());
  EXPECT_TRUE(multipart_lines[28].empty());  // The removed zip part.
  std::string final_delimiter = boundary;
  final_delimiter += "--";
  EXPECT_STREQ(final_delimiter.c_str(), multipart_lines[29].c_str());
  EXPECT_TRUE(multipart_lines[30].empty());
}
