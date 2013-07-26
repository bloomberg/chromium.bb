// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_browsertest_base.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"

const char WebRtcTestBase::kAudioVideoCallConstraints[] =
    "'{audio: true, video: true}'";
const char WebRtcTestBase::kAudioOnlyCallConstraints[] = "'{audio: true}'";
const char WebRtcTestBase::kVideoOnlyCallConstraints[] = "'{video: true}'";
const char WebRtcTestBase::kFailedWithErrorPermissionDenied[] =
    "failed-with-error-PERMISSION_DENIED";
const char WebRtcTestBase::kOkGotStream[] = "ok-got-stream";

// Convenience method which executes the provided javascript in the context
// of the provided web contents and returns what it evaluated to.
static std::string ExecuteJavascript(const std::string& javascript,
                                     content::WebContents* tab_contents) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents, javascript, &result));
  return result;
}

MediaStreamInfoBarDelegate* WebRtcTestBase::GetUserMediaAndWaitForInfobar(
    content::WebContents* tab_contents,
    const std::string& constraints) {
  content::WindowedNotificationObserver infobar_added(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::NotificationService::AllSources());

  // Request user media: this will launch the media stream info bar.
  GetUserMedia(tab_contents, constraints);

  // Wait for the bar to pop up, then return it.
  infobar_added.Wait();
  content::Details<InfoBarAddedDetails> details(infobar_added.details());
  MediaStreamInfoBarDelegate* media_infobar =
      details.ptr()->AsMediaStreamInfoBarDelegate();
  return media_infobar;
}

void WebRtcTestBase::CloseInfobarInTab(content::WebContents* tab_contents,
                                       MediaStreamInfoBarDelegate* infobar) {
  content::WindowedNotificationObserver infobar_removed(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::NotificationService::AllSources());

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(tab_contents);
  infobar_service->RemoveInfoBar(infobar);

  infobar_removed.Wait();
}

void WebRtcTestBase::GetUserMediaAndAccept(content::WebContents* tab_contents) {
  GetUserMediaWithSpecificConstraintsAndAccept(tab_contents,
                                               kAudioVideoCallConstraints);
}

void WebRtcTestBase::GetUserMediaWithSpecificConstraintsAndAccept(
    content::WebContents* tab_contents, const std::string& constraints) {
  MediaStreamInfoBarDelegate* media_infobar =
      GetUserMediaAndWaitForInfobar(tab_contents, constraints);

  media_infobar->Accept();

  CloseInfobarInTab(tab_contents, media_infobar);

  // Wait for WebRTC to call the success callback.
  EXPECT_TRUE(PollingWaitUntil(
      "obtainGetUserMediaResult()", kOkGotStream, tab_contents));
}

void WebRtcTestBase::GetUserMediaAndDeny(content::WebContents* tab_contents) {
  return GetUserMediaWithSpecificConstraintsAndDeny(tab_contents,
                                                    kAudioVideoCallConstraints);
}

void WebRtcTestBase::GetUserMediaWithSpecificConstraintsAndDeny(
    content::WebContents* tab_contents,
    const std::string& constraints) {
  MediaStreamInfoBarDelegate* media_infobar =
      GetUserMediaAndWaitForInfobar(tab_contents, constraints);

  media_infobar->Cancel();

  CloseInfobarInTab(tab_contents, media_infobar);

  // Wait for WebRTC to call the fail callback.
  EXPECT_TRUE(PollingWaitUntil("obtainGetUserMediaResult()",
                               kFailedWithErrorPermissionDenied,
                               tab_contents));
}

void WebRtcTestBase::GetUserMediaAndDismiss(
    content::WebContents* tab_contents) {
  MediaStreamInfoBarDelegate* media_infobar =
      GetUserMediaAndWaitForInfobar(tab_contents, kAudioVideoCallConstraints);

  media_infobar->InfoBarDismissed();

  CloseInfobarInTab(tab_contents, media_infobar);

  // A dismiss should be treated like a deny.
  EXPECT_TRUE(PollingWaitUntil("obtainGetUserMediaResult()",
                               kFailedWithErrorPermissionDenied,
                               tab_contents));
}

void WebRtcTestBase::GetUserMedia(content::WebContents* tab_contents,
                                  const std::string& constraints) {
  // Request user media: this will launch the media stream info bar.
  EXPECT_EQ("ok-requested",
            ExecuteJavascript(
                base::StringPrintf("getUserMedia(%s);", constraints.c_str()),
                tab_contents));
}

