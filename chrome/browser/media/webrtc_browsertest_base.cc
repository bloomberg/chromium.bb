// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_browsertest_base.h"

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"

const char WebRtcTestBase::kAudioVideoCallConstraints[] =
    "'{audio: true, video: true}'";
const char WebRtcTestBase::kAudioOnlyCallConstraints[] = "'{audio: true}'";
const char WebRtcTestBase::kVideoOnlyCallConstraints[] = "'{video: true}'";
const char WebRtcTestBase::kFailedWithPermissionDeniedError[] =
    "failed-with-error-PermissionDeniedError";

namespace {

base::LazyInstance<bool> hit_javascript_errors_ =
      LAZY_INSTANCE_INITIALIZER;

// Intercepts all log messages. We always attach this handler but only look at
// the results if the test requests so. Note that this will only work if the
// WebrtcTestBase-inheriting test cases do not run in parallel (if they did they
// would race to look at the log, which is global to all tests).
bool JavascriptErrorDetectingLogHandler(int severity,
                                        const char* file,
                                        int line,
                                        size_t message_start,
                                        const std::string& str) {
  if (file == NULL || std::string("CONSOLE") != file)
    return false;

  bool contains_uncaught = str.find("\"Uncaught ") != std::string::npos;
  if (severity == logging::LOG_ERROR ||
      (severity == logging::LOG_INFO && contains_uncaught)) {
    hit_javascript_errors_.Get() = true;
  }

  return false;
}

}  // namespace

WebRtcTestBase::WebRtcTestBase(): detect_errors_in_javascript_(false) {
  // The handler gets set for each test method, but that's fine since this
  // set operation is idempotent.
  logging::SetLogMessageHandler(&JavascriptErrorDetectingLogHandler);
  hit_javascript_errors_.Get() = false;

  EnablePixelOutput();
}

WebRtcTestBase::~WebRtcTestBase() {
  if (detect_errors_in_javascript_) {
    EXPECT_FALSE(hit_javascript_errors_.Get())
        << "Encountered javascript errors during test execution (Search "
        << "for Uncaught or ERROR:CONSOLE in the test output).";
  }
}

void WebRtcTestBase::GetUserMediaAndAccept(
    content::WebContents* tab_contents) const {
  GetUserMediaWithSpecificConstraintsAndAccept(tab_contents,
                                               kAudioVideoCallConstraints);
}

void WebRtcTestBase::GetUserMediaWithSpecificConstraintsAndAccept(
    content::WebContents* tab_contents,
    const std::string& constraints) const {
  InfoBar* infobar = GetUserMediaAndWaitForInfoBar(tab_contents, constraints);
  infobar->delegate()->AsConfirmInfoBarDelegate()->Accept();
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
    const std::string& constraints) const {
  InfoBar* infobar = GetUserMediaAndWaitForInfoBar(tab_contents, constraints);
  infobar->delegate()->AsConfirmInfoBarDelegate()->Cancel();
  CloseInfoBarInTab(tab_contents, infobar);

  // Wait for WebRTC to call the fail callback.
  EXPECT_TRUE(PollingWaitUntil("obtainGetUserMediaResult()",
                               kFailedWithPermissionDeniedError, tab_contents));
}

void WebRtcTestBase::GetUserMediaAndDismiss(
    content::WebContents* tab_contents) const {
  InfoBar* infobar =
      GetUserMediaAndWaitForInfoBar(tab_contents, kAudioVideoCallConstraints);
  infobar->delegate()->InfoBarDismissed();
  CloseInfoBarInTab(tab_contents, infobar);

  // A dismiss should be treated like a deny.
  EXPECT_TRUE(PollingWaitUntil("obtainGetUserMediaResult()",
                               kFailedWithPermissionDeniedError, tab_contents));
}

void WebRtcTestBase::GetUserMedia(content::WebContents* tab_contents,
                                  const std::string& constraints) const {
  // Request user media: this will launch the media stream info bar.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents, "doGetUserMedia(" + constraints + ");", &result));
  EXPECT_EQ("ok-requested", result);
}

InfoBar* WebRtcTestBase::GetUserMediaAndWaitForInfoBar(
    content::WebContents* tab_contents,
    const std::string& constraints) const {
  content::WindowedNotificationObserver infobar_added(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::NotificationService::AllSources());

  // Request user media: this will launch the media stream info bar.
  GetUserMedia(tab_contents, constraints);

  // Wait for the bar to pop up, then return it.
  infobar_added.Wait();
  content::Details<InfoBar::AddedDetails> details(infobar_added.details());
  EXPECT_TRUE(details->delegate()->AsMediaStreamInfoBarDelegate());
  return details.ptr();
}

content::WebContents* WebRtcTestBase::OpenPageAndGetUserMediaInNewTab(
      const GURL& url) const {
  chrome::AddTabAt(browser(), GURL(), -1, true);
  ui_test_utils::NavigateToURL(browser(), url);
#if defined (OS_LINUX)
  // Load the page again on Linux to work around crbug.com/281268.
  ui_test_utils::NavigateToURL(browser(), url);
#endif
  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(new_tab);
  return new_tab;
}

content::WebContents* WebRtcTestBase::OpenPageAndAcceptUserMedia(
    const GURL& url) const {
  content::WindowedNotificationObserver infobar_added(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::NotificationService::AllSources());

  ui_test_utils::NavigateToURL(browser(), url);

  infobar_added.Wait();

  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::Details<InfoBar::AddedDetails> details(infobar_added.details());
  InfoBar* infobar = details.ptr();
  EXPECT_TRUE(infobar);
  infobar->delegate()->AsMediaStreamInfoBarDelegate()->Accept();

  CloseInfoBarInTab(tab_contents, infobar);
  return tab_contents;
}

void WebRtcTestBase::CloseInfoBarInTab(
    content::WebContents* tab_contents,
    InfoBar* infobar) const {
  content::WindowedNotificationObserver infobar_removed(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::NotificationService::AllSources());

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(tab_contents);
  infobar_service->RemoveInfoBar(infobar);

  infobar_removed.Wait();
}

// Convenience method which executes the provided javascript in the context
// of the provided web contents and returns what it evaluated to.
std::string WebRtcTestBase::ExecuteJavascript(
    const std::string& javascript,
    content::WebContents* tab_contents) const {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents, javascript, &result));
  return result;
}

// The peer connection server lets our two tabs find each other and talk to
// each other (e.g. it is the application-specific "signaling solution").
void WebRtcTestBase::ConnectToPeerConnectionServer(
    const std::string& peer_name,
    content::WebContents* tab_contents) const {
  std::string javascript = base::StringPrintf(
      "connect('http://localhost:%s', '%s');",
      PeerConnectionServerRunner::kDefaultPort, peer_name.c_str());
  EXPECT_EQ("ok-connected", ExecuteJavascript(javascript, tab_contents));
}

void WebRtcTestBase::DetectErrorsInJavaScript() {
  detect_errors_in_javascript_ = true;
}

void WebRtcTestBase::StartDetectingVideo(
    content::WebContents* tab_contents,
    const std::string& video_element) const {
  std::string javascript = base::StringPrintf(
      "startDetection('%s', 320, 240)", video_element.c_str());
  EXPECT_EQ("ok-started", ExecuteJavascript(javascript, tab_contents));
}

void WebRtcTestBase::WaitForVideoToPlay(
    content::WebContents* tab_contents) const {
  EXPECT_TRUE(PollingWaitUntil("isVideoPlaying()", "video-playing",
                               tab_contents));
}
