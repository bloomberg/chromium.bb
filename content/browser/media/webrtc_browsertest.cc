// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/test/test_server.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

class WebrtcBrowserTest: public ContentBrowserTest {
 public:
  WebrtcBrowserTest() {}
  virtual ~WebrtcBrowserTest() {}

  virtual void SetUp() OVERRIDE {
    // We need fake devices in this test since we want to run on naked VMs. We
    // assume this switch is set by default in content_browsertests.
    ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));

    ASSERT_TRUE(test_server()->Start());
    ContentBrowserTest::SetUp();
  }
 protected:
  bool ExecuteJavascript(const std::string& javascript) {
    return ExecuteScript(shell()->web_contents(), javascript);
  }

  void ExpectTitle(const std::string& expected_title) const {
    string16 expected_title16(ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }
};

// These tests will all make a getUserMedia call with different constraints and
// see that the success callback is called. If the error callback is called or
// none of the callbacks are called the tests will simply time out and fail.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetVideoStreamAndStop) {
  GURL url(test_server()->GetURL("files/media/getusermedia_and_stop.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("getUserMedia({video: true});"));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetAudioAndVideoStreamAndStop) {
  GURL url(test_server()->GetURL("files/media/getusermedia_and_stop.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("getUserMedia({video: true, audio: true});"));

  ExpectTitle("OK");
}

// These tests will make a complete PeerConnection-based call and verify that
// video is playing for the call.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, CanSetupVideoCall) {
  GURL url(test_server()->GetURL("files/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("call({video: true});"));
  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, CanSetupAudioAndVideoCall) {
  GURL url(test_server()->GetURL("files/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("call({video: true, audio: true});"));
  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MANUAL_CanSetupCallAndSendDtmf) {
  GURL url(test_server()->GetURL("files/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(
      ExecuteJavascript("callAndSendDtmf('123,abc');"));
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       CanMakeEmptyCallThenAddStreamsAndRenegotiate) {
#if defined(OS_WIN)
  // This test is flaky on Win XP.
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    return;
#endif

  GURL url(test_server()->GetURL("files/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  const char* kJavascript =
      "makeEmptyCallThenAddOneStreamAndRenegotiate("
      "{video: true, audio: true});";
  EXPECT_TRUE(ExecuteJavascript(kJavascript));
  ExpectTitle("OK");
}

// This test will make a complete PeerConnection-based call but remove the
// MSID and bundle attribute from the initial offer to verify that
// video is playing for the call even if the initiating client don't support
// MSID. http://tools.ietf.org/html/draft-alvestrand-rtcweb-msid-02
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       CanSetupAudioAndVideoCallWithoutMsidAndBundle) {
  GURL url(test_server()->GetURL("files/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithoutMsidAndBundle();"));
  ExpectTitle("OK");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, CallWithDataOnly) {
  GURL url(test_server()->GetURL("files/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithDataOnly();"));
  ExpectTitle("OK");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel and audio and video tracks.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, CallWithDataAndMedia) {
  GURL url(test_server()->GetURL("files/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithDataAndMedia();"));
  ExpectTitle("OK");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel and later add an audio and video track.
// Flaky. http://crbug.com/175683
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       DISABLED_CallWithDataAndLaterAddMedia) {
  GURL url(test_server()->GetURL("files/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithDataAndLaterAddMedia();"));
  ExpectTitle("OK");
}

}  // namespace content

