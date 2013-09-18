// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_browsertest_base.h"

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
const char WebRtcTestBase::kFailedWithPermissionDeniedError[] =
    "failed-with-error-PermissionDeniedError";

WebRtcTestBase::WebRtcTestBase() {
}

WebRtcTestBase::~WebRtcTestBase() {
}

void WebRtcTestBase::GetUserMediaAndAccept(content::WebContents* tab_contents) {
  GetUserMediaWithSpecificConstraintsAndAccept(tab_contents,
                                               kAudioVideoCallConstraints);
}

void WebRtcTestBase::GetUserMediaWithSpecificConstraintsAndAccept(
    content::WebContents* tab_contents,
    const std::string& constraints) {
  MediaStreamInfoBarDelegate* infobar =
      GetUserMediaAndWaitForInfoBar(tab_contents, constraints);
  infobar->Accept();
  CloseInfoBarInTab(tab_contents, infobar);

  // Wait for WebRTC to call the success callback.
  const char kOkGotStream[] = "ok-got-stream";
  EXPECT_TRUE(PollingWaitUntil("obtainGetUserMediaResult()", kOkGotStream,
                               tab_contents));
}

void WebRtcTestBase::GetUserMediaAndDeny(content::WebContents* tab_contents) {
  return GetUserMediaWithSpecificConstraintsAndDeny(tab_contents,
                                                    kAudioVideoCallConstraints);
}

void WebRtcTestBase::GetUserMediaWithSpecificConstraintsAndDeny(
    content::WebContents* tab_contents,
    const std::string& constraints) {
  MediaStreamInfoBarDelegate* infobar =
      GetUserMediaAndWaitForInfoBar(tab_contents, constraints);
  infobar->Cancel();
  CloseInfoBarInTab(tab_contents, infobar);

  // Wait for WebRTC to call the fail callback.
  EXPECT_TRUE(PollingWaitUntil("obtainGetUserMediaResult()",
                               kFailedWithPermissionDeniedError, tab_contents));
}

void WebRtcTestBase::GetUserMediaAndDismiss(
    content::WebContents* tab_contents) {
  MediaStreamInfoBarDelegate* infobar =
      GetUserMediaAndWaitForInfoBar(tab_contents, kAudioVideoCallConstraints);
  infobar->InfoBarDismissed();
  CloseInfoBarInTab(tab_contents, infobar);

  // A dismiss should be treated like a deny.
  EXPECT_TRUE(PollingWaitUntil("obtainGetUserMediaResult()",
                               kFailedWithPermissionDeniedError, tab_contents));
}

void WebRtcTestBase::GetUserMedia(content::WebContents* tab_contents,
                                  const std::string& constraints) {
  // Request user media: this will launch the media stream info bar.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents, "doGetUserMedia(" + constraints + ");", &result));
  EXPECT_EQ("ok-requested", result);
}

MediaStreamInfoBarDelegate* WebRtcTestBase::GetUserMediaAndWaitForInfoBar(
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
  MediaStreamInfoBarDelegate* infobar = details->AsMediaStreamInfoBarDelegate();
  EXPECT_TRUE(infobar);
  return infobar;
}

void WebRtcTestBase::CloseInfoBarInTab(content::WebContents* tab_contents,
                                       MediaStreamInfoBarDelegate* infobar) {
  content::WindowedNotificationObserver infobar_removed(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::NotificationService::AllSources());

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(tab_contents);
  infobar_service->RemoveInfoBar(infobar);

  infobar_removed.Wait();
}
