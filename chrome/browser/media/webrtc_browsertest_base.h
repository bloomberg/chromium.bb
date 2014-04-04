// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_BASE_H_

#include <string>

#include "chrome/test/base/in_process_browser_test.h"

class InfoBar;

namespace content {
class WebContents;
}

// Base class for WebRTC browser tests with useful primitives for interacting
// getUserMedia. We use inheritance here because it makes the test code look
// as clean as it can be.
class WebRtcTestBase : public InProcessBrowserTest {
 protected:
  // Typical constraints.
  static const char kAudioVideoCallConstraints[];
  static const char kAudioOnlyCallConstraints[];
  static const char kVideoOnlyCallConstraints[];
  static const char kAudioVideoCallConstraintsQVGA[];
  static const char kAudioVideoCallConstraints360p[];
  static const char kAudioVideoCallConstraintsVGA[];
  static const char kAudioVideoCallConstraints720p[];
  static const char kAudioVideoCallConstraints1080p[];

  static const char kFailedWithPermissionDeniedError[];
  static const char kFailedWithPermissionDismissedError[];

  WebRtcTestBase();
  virtual ~WebRtcTestBase();

  // These all require that the loaded page fulfills the public interface in
  // chrome/test/data/webrtc/message_handling.js.
  void GetUserMediaAndAccept(content::WebContents* tab_contents) const;
  void GetUserMediaWithSpecificConstraintsAndAccept(
      content::WebContents* tab_contents,
      const std::string& constraints) const;
  void GetUserMediaAndDeny(content::WebContents* tab_contents);
  void GetUserMediaWithSpecificConstraintsAndDeny(
      content::WebContents* tab_contents,
      const std::string& constraints) const;
  void GetUserMediaAndDismiss(content::WebContents* tab_contents) const;
  void GetUserMedia(content::WebContents* tab_contents,
                    const std::string& constraints) const;

  // Convenience method which opens the page at url, calls GetUserMediaAndAccept
  // and returns the new tab.
  content::WebContents* OpenPageAndGetUserMediaInNewTab(const GURL& url) const;

  // Convenience method which opens the page at url, calls
  // GetUserMediaAndAcceptWithSpecificConstraints and returns the new tab.
  content::WebContents* OpenPageAndGetUserMediaInNewTabWithConstraints(
      const GURL& url, const std::string& constraints) const;

  // Convenience method which gets the URL for |test_page| and calls
  // OpenPageAndGetUserMediaInNewTab().
  content::WebContents* OpenTestPageAndGetUserMediaInNewTab(
    const std::string& test_page) const;

  // Opens the page at |url| where getUserMedia has been invoked through other
  // means and accepts the user media request.
  content::WebContents* OpenPageAndAcceptUserMedia(const GURL& url) const;

  // Closes the last local stream acquired by the GetUserMedia* methods.
  void CloseLastLocalStream(content::WebContents* tab_contents) const;

  void ConnectToPeerConnectionServer(const std::string& peer_name,
                                     content::WebContents* tab_contents) const;
  std::string ExecuteJavascript(const std::string& javascript,
                                content::WebContents* tab_contents) const;

  void EstablishCall(content::WebContents* from_tab,
                     content::WebContents* to_tab) const;

  void HangUp(content::WebContents* from_tab) const;

  void WaitUntilHangupVerified(content::WebContents* tab_contents) const;

  // Call this to enable monitoring of javascript errors for this test method.
  // This will only work if the tests are run sequentially by the test runner
  // (i.e. with --test-launcher-developer-mode or --test-launcher-jobs=1).
  void DetectErrorsInJavaScript();

  // Methods for detecting if video is playing (the loaded page must have
  // chrome/test/data/webrtc/video_detector.js and its dependencies loaded to
  // make that work). Looks at a 320x240 area of the target video tag.
  void StartDetectingVideo(content::WebContents* tab_contents,
                           const std::string& video_element) const;
  void WaitForVideoToPlay(content::WebContents* tab_contents) const;

  // Returns the stream size as a string on the format <width>x<height>.
  std::string GetStreamSize(content::WebContents* tab_contents,
                            const std::string& video_element) const;

  // Methods to check what devices we have on the system.
  bool HasWebcamAvailableOnSystem(content::WebContents* tab_contents) const;

 private:
  void CloseInfoBarInTab(content::WebContents* tab_contents,
                         InfoBar* infobar) const;
  InfoBar* GetUserMediaAndWaitForInfoBar(content::WebContents* tab_contents,
                                         const std::string& constraints) const;

  bool detect_errors_in_javascript_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcTestBase);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_BASE_H_
