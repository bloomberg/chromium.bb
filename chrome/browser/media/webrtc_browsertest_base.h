// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_BASE_H_

#include <string>

#include "chrome/test/base/in_process_browser_test.h"

namespace content {
class WebContents;
}

class MediaStreamInfoBarDelegate;

// Base class for WebRTC browser tests with useful primitives for interacting
// getUserMedia. We use inheritance here because it makes the test code look
// as clean as it can be.
class WebRtcTestBase : public InProcessBrowserTest {
 protected:
  // Typical constraints.
  static const char kAudioVideoCallConstraints[];
  static const char kAudioOnlyCallConstraints[];
  static const char kVideoOnlyCallConstraints[];

  static const char kFailedWithPermissionDeniedError[];

  WebRtcTestBase();
  virtual ~WebRtcTestBase();

  void GetUserMediaAndAccept(content::WebContents* tab_contents);
  void GetUserMediaWithSpecificConstraintsAndAccept(
      content::WebContents* tab_contents,
      const std::string& constraints);
  void GetUserMediaAndDeny(content::WebContents* tab_contents);
  void GetUserMediaWithSpecificConstraintsAndDeny(
      content::WebContents* tab_contents,
      const std::string& constraints);
  void GetUserMediaAndDismiss(content::WebContents* tab_contents);
  void GetUserMedia(content::WebContents* tab_contents,
                    const std::string& constraints);

 private:
  MediaStreamInfoBarDelegate* GetUserMediaAndWaitForInfoBar(
      content::WebContents* tab_contents,
      const std::string& constraints);
  void CloseInfoBarInTab(content::WebContents* tab_contents,
                         MediaStreamInfoBarDelegate* infobar);

  DISALLOW_COPY_AND_ASSIGN(WebRtcTestBase);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_BASE_H_
