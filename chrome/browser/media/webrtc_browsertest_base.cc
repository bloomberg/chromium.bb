// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_browsertest_base.h"

#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_WIN)
// For fine-grained suppression.
#include "base/win/windows_version.h"
#endif

const char WebRtcTestBase::kAudioVideoCallConstraints[] =
    "{audio: true, video: true}";
const char WebRtcTestBase::kAudioVideoCallConstraintsQVGA[] =
   "{audio: true, video: {mandatory: {minWidth: 320, maxWidth: 320, "
   " minHeight: 240, maxHeight: 240}}}";
const char WebRtcTestBase::kAudioVideoCallConstraints360p[] =
   "{audio: true, video: {mandatory: {minWidth: 640, maxWidth: 640, "
   " minHeight: 360, maxHeight: 360}}}";
const char WebRtcTestBase::kAudioVideoCallConstraintsVGA[] =
   "{audio: true, video: {mandatory: {minWidth: 640, maxWidth: 640, "
   " minHeight: 480, maxHeight: 480}}}";
const char WebRtcTestBase::kAudioVideoCallConstraints720p[] =
   "{audio: true, video: {mandatory: {minWidth: 1280, maxWidth: 1280, "
   " minHeight: 720, maxHeight: 720}}}";
const char WebRtcTestBase::kAudioVideoCallConstraints1080p[] =
    "{audio: true, video: {mandatory: {minWidth: 1920, maxWidth: 1920, "
    " minHeight: 1080, maxHeight: 1080}}}";
const char WebRtcTestBase::kAudioOnlyCallConstraints[] = "{audio: true}";
const char WebRtcTestBase::kVideoOnlyCallConstraints[] = "{video: true}";
const char WebRtcTestBase::kFailedWithPermissionDeniedError[] =
    "failed-with-error-PermissionDeniedError";
const char WebRtcTestBase::kFailedWithPermissionDismissedError[] =
    "failed-with-error-PermissionDismissedError";

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
  infobars::InfoBar* infobar =
      GetUserMediaAndWaitForInfoBar(tab_contents, constraints);
  infobar->delegate()->AsConfirmInfoBarDelegate()->Accept();
  CloseInfoBarInTab(tab_contents, infobar);

  // Wait for WebRTC to call the success callback.
  const char kOkGotStream[] = "ok-got-stream";
  EXPECT_TRUE(test::PollingWaitUntil("obtainGetUserMediaResult()", kOkGotStream,
                                     tab_contents));
}

void WebRtcTestBase::GetUserMediaAndDeny(content::WebContents* tab_contents) {
  return GetUserMediaWithSpecificConstraintsAndDeny(tab_contents,
                                                    kAudioVideoCallConstraints);
}

void WebRtcTestBase::GetUserMediaWithSpecificConstraintsAndDeny(
    content::WebContents* tab_contents,
    const std::string& constraints) const {
  infobars::InfoBar* infobar =
      GetUserMediaAndWaitForInfoBar(tab_contents, constraints);
  infobar->delegate()->AsConfirmInfoBarDelegate()->Cancel();
  CloseInfoBarInTab(tab_contents, infobar);

  // Wait for WebRTC to call the fail callback.
  EXPECT_TRUE(test::PollingWaitUntil("obtainGetUserMediaResult()",
                                     kFailedWithPermissionDeniedError,
                                     tab_contents));
}

void WebRtcTestBase::GetUserMediaAndDismiss(
    content::WebContents* tab_contents) const {
  infobars::InfoBar* infobar =
      GetUserMediaAndWaitForInfoBar(tab_contents, kAudioVideoCallConstraints);
  infobar->delegate()->InfoBarDismissed();
  CloseInfoBarInTab(tab_contents, infobar);

  // A dismiss should be treated like a deny.
  EXPECT_TRUE(test::PollingWaitUntil("obtainGetUserMediaResult()",
                                     kFailedWithPermissionDismissedError,
                                     tab_contents));
}

void WebRtcTestBase::GetUserMedia(content::WebContents* tab_contents,
                                  const std::string& constraints) const {
  // Request user media: this will launch the media stream info bar.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents, "doGetUserMedia(" + constraints + ");", &result));
  EXPECT_EQ("ok-requested", result);
}

infobars::InfoBar* WebRtcTestBase::GetUserMediaAndWaitForInfoBar(
    content::WebContents* tab_contents,
    const std::string& constraints) const {
  content::WindowedNotificationObserver infobar_added(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::NotificationService::AllSources());

  // Request user media: this will launch the media stream info bar.
  GetUserMedia(tab_contents, constraints);

  // Wait for the bar to pop up, then return it.
  infobar_added.Wait();
  content::Details<infobars::InfoBar::AddedDetails> details(
      infobar_added.details());
  EXPECT_TRUE(details->delegate()->AsMediaStreamInfoBarDelegate());
  return details.ptr();
}

content::WebContents* WebRtcTestBase::OpenPageAndGetUserMediaInNewTab(
    const GURL& url) const {
  return OpenPageAndGetUserMediaInNewTabWithConstraints(
      url, kAudioVideoCallConstraints);
}

content::WebContents*
WebRtcTestBase::OpenPageAndGetUserMediaInNewTabWithConstraints(
    const GURL& url,
    const std::string& constraints) const {
  chrome::AddTabAt(browser(), GURL(), -1, true);
  ui_test_utils::NavigateToURL(browser(), url);
#if defined (OS_LINUX)
  // Load the page again on Linux to work around crbug.com/281268.
  ui_test_utils::NavigateToURL(browser(), url);
#endif
  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaWithSpecificConstraintsAndAccept(new_tab, constraints);
  return new_tab;
}

content::WebContents* WebRtcTestBase::OpenTestPageAndGetUserMediaInNewTab(
    const std::string& test_page) const {
  return OpenPageAndGetUserMediaInNewTab(
      embedded_test_server()->GetURL(test_page));
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
  content::Details<infobars::InfoBar::AddedDetails> details(
      infobar_added.details());
  infobars::InfoBar* infobar = details.ptr();
  EXPECT_TRUE(infobar);
  infobar->delegate()->AsMediaStreamInfoBarDelegate()->Accept();

  CloseInfoBarInTab(tab_contents, infobar);
  return tab_contents;
}

void WebRtcTestBase::CloseInfoBarInTab(content::WebContents* tab_contents,
                                       infobars::InfoBar* infobar) const {
  content::WindowedNotificationObserver infobar_removed(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::NotificationService::AllSources());

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(tab_contents);
  infobar_service->RemoveInfoBar(infobar);

  infobar_removed.Wait();
}

void WebRtcTestBase::CloseLastLocalStream(
    content::WebContents* tab_contents) const {
  EXPECT_EQ("ok-stopped",
            ExecuteJavascript("stopLocalStream();", tab_contents));
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

void WebRtcTestBase::SetupPeerconnectionWithLocalStream(
    content::WebContents* tab) const {
  EXPECT_EQ("ok-peerconnection-created",
            ExecuteJavascript("preparePeerConnection()", tab));
  EXPECT_EQ("ok-added", ExecuteJavascript("addLocalStream()", tab));
}

std::string WebRtcTestBase::CreateLocalOffer(
      content::WebContents* from_tab) const {
  std::string response = ExecuteJavascript("createLocalOffer({})", from_tab);
  EXPECT_EQ("ok-", response.substr(0, 3)) << "Failed to create local offer: "
      << response;

  std::string local_offer = response.substr(3);
  return local_offer;
}

std::string WebRtcTestBase::CreateAnswer(std::string local_offer,
                                         content::WebContents* to_tab) const {
  std::string javascript =
      base::StringPrintf("receiveOfferFromPeer('%s', {})", local_offer.c_str());
  std::string response = ExecuteJavascript(javascript, to_tab);
  EXPECT_EQ("ok-", response.substr(0, 3))
      << "Receiving peer failed to receive offer and create answer: "
      << response;

  std::string answer = response.substr(3);
  return answer;
}

void WebRtcTestBase::ReceiveAnswer(std::string answer,
                                   content::WebContents* from_tab) const {
  ASSERT_EQ(
      "ok-accepted-answer",
      ExecuteJavascript(
          base::StringPrintf("receiveAnswerFromPeer('%s')", answer.c_str()),
          from_tab));
}

void WebRtcTestBase::GatherAndSendIceCandidates(
    content::WebContents* from_tab,
    content::WebContents* to_tab) const {
  std::string ice_candidates =
      ExecuteJavascript("getAllIceCandidates()", from_tab);

  EXPECT_EQ("ok-received-candidates", ExecuteJavascript(
      base::StringPrintf("receiveIceCandidates('%s')", ice_candidates.c_str()),
      to_tab));
}

void WebRtcTestBase::NegotiateCall(content::WebContents* from_tab,
                                   content::WebContents* to_tab) const {
  std::string local_offer = CreateLocalOffer(from_tab);
  std::string answer = CreateAnswer(local_offer, to_tab);
  ReceiveAnswer(answer, from_tab);

  // Send all ICE candidates (wait for gathering to finish if necessary).
  GatherAndSendIceCandidates(to_tab, from_tab);
  GatherAndSendIceCandidates(from_tab, to_tab);
}

void WebRtcTestBase::HangUp(content::WebContents* from_tab) const {
  EXPECT_EQ("ok-call-hung-up", ExecuteJavascript("hangUp()", from_tab));
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
  EXPECT_TRUE(test::PollingWaitUntil("isVideoPlaying()", "video-playing",
                                     tab_contents));
}

std::string WebRtcTestBase::GetStreamSize(
    content::WebContents* tab_contents,
    const std::string& video_element) const {
  std::string javascript =
      base::StringPrintf("getStreamSize('%s')", video_element.c_str());
  std::string result = ExecuteJavascript(javascript, tab_contents);
  EXPECT_TRUE(StartsWithASCII(result, "ok-", true));
  return result.substr(3);
}

bool WebRtcTestBase::HasWebcamAvailableOnSystem(
    content::WebContents* tab_contents) const {
  std::string result =
      ExecuteJavascript("hasVideoInputDeviceOnSystem();", tab_contents);
  return result == "has-video-input-device";
}

bool WebRtcTestBase::OnWinXp() const {
#if defined(OS_WIN)
  return base::win::GetVersion() <= base::win::VERSION_XP;
#else
  return false;
#endif
}

bool WebRtcTestBase::OnWin8() const {
#if defined(OS_WIN)
  return base::win::GetVersion() > base::win::VERSION_WIN7;
#else
  return false;
#endif
}
