// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/spawned_test_server.h"

static const base::FilePath::CharType kPeerConnectionServer[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("peerconnection_server.exe");
#else
    FILE_PATH_LITERAL("peerconnection_server");
#endif

static const char kMainWebrtcTestHtmlPage[] =
    "files/webrtc/webrtc_jsep01_test.html";

// Top-level integration test for WebRTC. Requires a real webcam and microphone
// on the running system. This test is not meant to run in the main browser
// test suite since normal tester machines do not have webcams.
class WebrtcBrowserTest : public InProcessBrowserTest {
 public:
  WebrtcBrowserTest() : peerconnection_server_(0) {}

  virtual void SetUp() OVERRIDE {
    RunPeerConnectionServer();
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    ShutdownPeerConnectionServer();
    InProcessBrowserTest::TearDown();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // TODO(phoglund): check that user actually has the requisite devices and
    // print a nice message if not; otherwise the test just times out which can
    // be confusing.
    // This test expects real device handling and requires a real webcam / audio
    // device; it will not work with fake devices.
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
  }

  // TODO(phoglund): This ugly poll method is only here while we transition
  // the test javascript to just post events when things happen. Right now they
  // don't because the webrtc_call.py and other tests use this polling way of
  // communicating when we are waiting from an asynchronous event in the
  // javascript. This method is meant to emulate WaitUntil in the PyAuto
  // framework.
  bool UglyPollingWaitUntil(const std::string& javascript,
                            const std::string& evaluates_to,
                            content::WebContents* tab_contents) {
    base::Time start_time = base::Time::Now();
    base::TimeDelta timeout = base::TimeDelta::FromSeconds(20);
    std::string result;

    while (base::Time::Now() - start_time < timeout) {
      result = ExecuteJavascript(javascript, tab_contents);
      if (evaluates_to == result)
        return true;
    }
    LOG(ERROR) << "Timed out while waiting for " << javascript <<
        " to evaluate to " << evaluates_to << "; last result was '" << result <<
        "'";
    return false;
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

  void GetUserMedia(content::WebContents* tab_contents) {
    content::WindowedNotificationObserver infobar_added(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());

    // Request user media: this will launch the media stream info bar.
    EXPECT_EQ("ok-requested", ExecuteJavascript(
        "getUserMedia('{video: true, audio: true}');", tab_contents));

    // Wait for the bar to pop up, then accept.
    infobar_added.Wait();
    content::Details<InfoBarAddedDetails> details(infobar_added.details());
    MediaStreamInfoBarDelegate* media_infobar =
        details.ptr()->AsMediaStreamInfoBarDelegate();
    media_infobar->Accept();

    // Wait for WebRTC to call the success callback.
    EXPECT_TRUE(UglyPollingWaitUntil("obtainGetUserMediaResult();",
                                     "ok-got-stream",
                                     tab_contents));
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
                     content::WebContents* to_tab) {
    EXPECT_EQ("ok-peerconnection-created",
              ExecuteJavascript("preparePeerConnection()", from_tab));
    EXPECT_EQ("ok-added",
              ExecuteJavascript("addLocalStream()", from_tab));
    EXPECT_EQ("ok-negotiating",
              ExecuteJavascript("negotiateCall()", from_tab));

    // Ensure the call gets up on both sides.
    EXPECT_TRUE(UglyPollingWaitUntil("getPeerConnectionReadyState()",
                                     "active", from_tab));
    EXPECT_TRUE(UglyPollingWaitUntil("getPeerConnectionReadyState()",
                                     "active", to_tab));
  }

  void StartDetectingVideo(content::WebContents* tab_contents,
                           const std::string& video_element) {
    std::string javascript = base::StringPrintf(
        "startDetection('%s', 'frame-buffer', 320, 240)",
        video_element.c_str());
    EXPECT_EQ("ok-started", ExecuteJavascript(javascript, tab_contents));
  }

  void WaitForVideo(content::WebContents* tab_contents) {
    EXPECT_TRUE(UglyPollingWaitUntil("isVideoPlaying()", "video-playing",
                                     tab_contents));
  }

  void HangUp(content::WebContents* from_tab) {
    EXPECT_EQ("ok-call-hung-up", ExecuteJavascript("hangUp()", from_tab));
  }

  void WaitUntilHangupVerified(content::WebContents* tab_contents) {
    EXPECT_TRUE(UglyPollingWaitUntil("getPeerConnectionReadyState()",
                                     "no-peer-connection", tab_contents));
  }

 private:
  void RunPeerConnectionServer() {
    base::FilePath peerconnection_server;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &peerconnection_server));
    peerconnection_server = peerconnection_server.Append(kPeerConnectionServer);

    EXPECT_TRUE(file_util::PathExists(peerconnection_server)) <<
        "Missing peerconnection_server. You must build "
        "it so it ends up next to the browser test binary.";
    EXPECT_TRUE(base::LaunchProcess(
        CommandLine(peerconnection_server),
        base::LaunchOptions(),
        &peerconnection_server_)) << "Failed to launch peerconnection_server.";
  }

  void ShutdownPeerConnectionServer() {
    EXPECT_TRUE(base::KillProcess(peerconnection_server_, 0, false)) <<
        "Failed to shut down peerconnection_server!";
  }

  base::ProcessHandle peerconnection_server_;
};

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MANUAL_RunsAudioVideoWebRTCCallInTwoTabs) {
  EXPECT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL(kMainWebrtcTestHtmlPage));
  content::WebContents* left_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMedia(left_tab);

  chrome::AddBlankTabAt(browser(), -1, true);
  content::WebContents* right_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
        browser(), test_server()->GetURL(kMainWebrtcTestHtmlPage));
  GetUserMedia(right_tab);

  ConnectToPeerConnectionServer("peer 1", left_tab);
  ConnectToPeerConnectionServer("peer 2", right_tab);

  EstablishCall(left_tab, right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideo(left_tab);
  WaitForVideo(right_tab);

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);
}
