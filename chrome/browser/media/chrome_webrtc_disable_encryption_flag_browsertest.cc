// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/common/content_switches.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

using chrome::VersionInfo;

// This tests the --disable-webrtc-encryption command line flag. Disabling
// encryption should only be possible on certain channels.

// NOTE: The test case for each channel will only be exercised when the browser
// is actually built for that channel. This is not ideal. One can test manually
// by e.g. faking the channel returned in VersionInfo::GetChannel(). It's
// likely good to have the test anyway, even though a failure might not be
// detected until a branch has been promoted to another channel. The unit
// test for ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch
// tests for all channels however.
// TODO(grunell): Test the different channel cases for any build.
class WebRtcDisableEncryptionFlagBrowserTest : public WebRtcTestBase {
 public:
  WebRtcDisableEncryptionFlagBrowserTest() {}
  virtual ~WebRtcDisableEncryptionFlagBrowserTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // This test should run with fake devices.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // Disable encryption with the command line flag.
    command_line->AppendSwitch(switches::kDisableWebRtcEncryption);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcDisableEncryptionFlagBrowserTest);
};

// Makes a call and checks that there's encryption or not in the SDP offer.
IN_PROC_BROWSER_TEST_F(WebRtcDisableEncryptionFlagBrowserTest,
                       VerifyEncryption) {
  if (!OnWinXp())
    return;  // Flaky timeout on a webrtc Win XP bot. http://crbug.com/368163.

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  content::WebContents* left_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  content::WebContents* right_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

  SetupPeerconnectionWithLocalStream(left_tab);
  SetupPeerconnectionWithLocalStream(right_tab);

  NegotiateCall(left_tab, right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideoToPlay(left_tab);
  WaitForVideoToPlay(right_tab);

  bool should_detect_encryption = true;
  VersionInfo::Channel channel = VersionInfo::GetChannel();
  if (channel == VersionInfo::CHANNEL_UNKNOWN ||
      channel == VersionInfo::CHANNEL_CANARY ||
      channel == VersionInfo::CHANNEL_DEV) {
    should_detect_encryption = false;
  }
#if defined(OS_ANDROID)
  if (channel == VersionInfo::CHANNEL_BETA)
    should_detect_encryption = false;
#endif

  std::string expected_string = should_detect_encryption ?
    "crypto-seen" : "no-crypto-seen";

  ASSERT_EQ(expected_string,
            ExecuteJavascript("hasSeenCryptoInSdp()", left_tab));

  HangUp(left_tab);
  HangUp(right_tab);
}
