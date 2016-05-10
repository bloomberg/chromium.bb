// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_browsertest_base.h"

#include <stddef.h>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_WIN)
// For fine-grained suppression.
#include "base/win/windows_version.h"
#endif

const char WebRtcTestBase::kAudioVideoCallConstraints[] =
    "{audio: true, video: true}";
const char WebRtcTestBase::kVideoCallConstraintsQVGA[] =
   "{video: {mandatory: {minWidth: 320, maxWidth: 320, "
   " minHeight: 240, maxHeight: 240}}}";
const char WebRtcTestBase::kVideoCallConstraints360p[] =
   "{video: {mandatory: {minWidth: 640, maxWidth: 640, "
   " minHeight: 360, maxHeight: 360}}}";
const char WebRtcTestBase::kVideoCallConstraintsVGA[] =
   "{video: {mandatory: {minWidth: 640, maxWidth: 640, "
   " minHeight: 480, maxHeight: 480}}}";
const char WebRtcTestBase::kVideoCallConstraints720p[] =
   "{video: {mandatory: {minWidth: 1280, maxWidth: 1280, "
   " minHeight: 720, maxHeight: 720}}}";
const char WebRtcTestBase::kVideoCallConstraints1080p[] =
    "{video: {mandatory: {minWidth: 1920, maxWidth: 1920, "
    " minHeight: 1080, maxHeight: 1080}}}";
const char WebRtcTestBase::kAudioOnlyCallConstraints[] = "{audio: true}";
const char WebRtcTestBase::kVideoOnlyCallConstraints[] = "{video: true}";
const char WebRtcTestBase::kOkGotStream[] = "ok-got-stream";
const char WebRtcTestBase::kFailedWithPermissionDeniedError[] =
    "failed-with-error-PermissionDeniedError";
const char WebRtcTestBase::kFailedWithPermissionDismissedError[] =
    "failed-with-error-PermissionDismissedError";
const char WebRtcTestBase::kAudioVideoCallConstraints360p[] =
   "{audio: true, video: {mandatory: {minWidth: 640, maxWidth: 640, "
   " minHeight: 360, maxHeight: 360}}}";
const char WebRtcTestBase::kAudioVideoCallConstraints720p[] =
   "{audio: true, video: {mandatory: {minWidth: 1280, maxWidth: 1280, "
   " minHeight: 720, maxHeight: 720}}}";
const char WebRtcTestBase::kUseDefaultCertKeygen[] = "null";
const char WebRtcTestBase::kUseDefaultVideoCodec[] = "";

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

// PermissionRequestObserver ---------------------------------------------------

// Used to observe the creation of permission prompt without responding.
class PermissionRequestObserver : public PermissionBubbleManager::Observer {
 public:
  explicit PermissionRequestObserver(content::WebContents* web_contents)
      : bubble_manager_(PermissionBubbleManager::FromWebContents(web_contents)),
        request_shown_(false),
        message_loop_runner_(new content::MessageLoopRunner) {
    bubble_manager_->AddObserver(this);
  }
  ~PermissionRequestObserver() override {
    // Safe to remove twice if it happens.
    bubble_manager_->RemoveObserver(this);
  }

  void Wait() { message_loop_runner_->Run(); }

  bool request_shown() const { return request_shown_; }

 private:
  // PermissionBubbleManager::Observer
  void OnBubbleAdded() override {
    request_shown_ = true;
    bubble_manager_->RemoveObserver(this);
    message_loop_runner_->Quit();
  }

  PermissionBubbleManager* bubble_manager_;
  bool request_shown_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestObserver);
};

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

bool WebRtcTestBase::GetUserMediaAndAccept(
    content::WebContents* tab_contents) const {
  return GetUserMediaWithSpecificConstraintsAndAccept(
      tab_contents, kAudioVideoCallConstraints);
}

bool WebRtcTestBase::GetUserMediaWithSpecificConstraintsAndAccept(
    content::WebContents* tab_contents,
    const std::string& constraints) const {
  std::string result;
  PermissionBubbleManager::FromWebContents(tab_contents)
      ->set_auto_response_for_test(PermissionBubbleManager::ACCEPT_ALL);
  PermissionRequestObserver permissionRequestObserver(tab_contents);
  GetUserMedia(tab_contents, constraints);
  EXPECT_TRUE(permissionRequestObserver.request_shown());
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents->GetMainFrame(), "obtainGetUserMediaResult();", &result));
  return kOkGotStream == result;
}

bool WebRtcTestBase::GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(
    content::WebContents* tab_contents,
    const std::string& constraints) const {
  std::string result;
  PermissionBubbleManager::FromWebContents(tab_contents)
      ->set_auto_response_for_test(PermissionBubbleManager::ACCEPT_ALL);
  GetUserMedia(tab_contents, constraints);
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents->GetMainFrame(), "obtainGetUserMediaResult();", &result));
  return kOkGotStream == result;
}

void WebRtcTestBase::GetUserMediaAndDeny(content::WebContents* tab_contents) {
  return GetUserMediaWithSpecificConstraintsAndDeny(tab_contents,
                                                    kAudioVideoCallConstraints);
}

void WebRtcTestBase::GetUserMediaWithSpecificConstraintsAndDeny(
    content::WebContents* tab_contents,
    const std::string& constraints) const {
  std::string result;
  PermissionBubbleManager::FromWebContents(tab_contents)
      ->set_auto_response_for_test(PermissionBubbleManager::DENY_ALL);
  PermissionRequestObserver permissionRequestObserver(tab_contents);
  GetUserMedia(tab_contents, constraints);
  EXPECT_TRUE(permissionRequestObserver.request_shown());
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents->GetMainFrame(), "obtainGetUserMediaResult();", &result));
  EXPECT_EQ(kFailedWithPermissionDeniedError, result);
}

void WebRtcTestBase::GetUserMediaAndDismiss(
    content::WebContents* tab_contents) const {
  std::string result;
  PermissionBubbleManager::FromWebContents(tab_contents)
      ->set_auto_response_for_test(PermissionBubbleManager::DISMISS);
  PermissionRequestObserver permissionRequestObserver(tab_contents);
  GetUserMedia(tab_contents, kAudioVideoCallConstraints);
  EXPECT_TRUE(permissionRequestObserver.request_shown());
  // A dismiss should be treated like a deny.
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents->GetMainFrame(), "obtainGetUserMediaResult();", &result));
  EXPECT_EQ(kFailedWithPermissionDismissedError, result);
}

void WebRtcTestBase::GetUserMediaAndExpectAutoAcceptWithoutPrompt(
    content::WebContents* tab_contents) const {
  std::string result;
  // We issue a GetUserMedia() request. We expect that the origin already has a
  // sticky "accept" permission (e.g. because the caller previously called
  // GetUserMediaAndAccept()), and therefore the GetUserMedia() request
  // automatically succeeds without a prompt.
  // If the caller made a mistake, a prompt may show up instead. For this case,
  // we set an auto-response to avoid leaving the prompt hanging. The choice of
  // DENY_ALL makes sure that the response to the prompt doesn't accidentally
  // result in a newly granted media stream permission.
  PermissionBubbleManager::FromWebContents(tab_contents)
      ->set_auto_response_for_test(PermissionBubbleManager::DENY_ALL);
  PermissionRequestObserver permissionRequestObserver(tab_contents);
  GetUserMedia(tab_contents, kAudioVideoCallConstraints);
  EXPECT_FALSE(permissionRequestObserver.request_shown());
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents->GetMainFrame(), "obtainGetUserMediaResult();", &result));
  EXPECT_EQ(kOkGotStream, result);
}

void WebRtcTestBase::GetUserMediaAndExpectAutoDenyWithoutPrompt(
    content::WebContents* tab_contents) const {
  std::string result;
  // We issue a GetUserMedia() request. We expect that the origin already has a
  // sticky "deny" permission (e.g. because the caller previously called
  // GetUserMediaAndDeny()), and therefore the GetUserMedia() request
  // automatically succeeds without a prompt.
  // If the caller made a mistake, a prompt may show up instead. For this case,
  // we set an auto-response to avoid leaving the prompt hanging. The choice of
  // ACCEPT_ALL makes sure that the response to the prompt doesn't accidentally
  // result in a newly granted media stream permission.
  PermissionBubbleManager::FromWebContents(tab_contents)
      ->set_auto_response_for_test(PermissionBubbleManager::ACCEPT_ALL);
  PermissionRequestObserver permissionRequestObserver(tab_contents);
  GetUserMedia(tab_contents, kAudioVideoCallConstraints);
  EXPECT_FALSE(permissionRequestObserver.request_shown());
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents->GetMainFrame(), "obtainGetUserMediaResult();", &result));
  EXPECT_EQ(kFailedWithPermissionDeniedError, result);
}

void WebRtcTestBase::GetUserMedia(content::WebContents* tab_contents,
                                  const std::string& constraints) const {
  // Request user media: this will launch the media stream info bar or bubble.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab_contents, "doGetUserMedia(" + constraints + ");", &result));
  EXPECT_TRUE(result == "request-callback-denied" ||
              result == "request-callback-granted");
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
  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Accept if necessary, but don't expect a prompt (because auto-accept is also
  // okay).
  PermissionBubbleManager::FromWebContents(new_tab)
      ->set_auto_response_for_test(PermissionBubbleManager::ACCEPT_ALL);
  GetUserMedia(new_tab, constraints);
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      new_tab->GetMainFrame(), "obtainGetUserMediaResult();", &result));
  EXPECT_EQ(kOkGotStream, result);
  return new_tab;
}

content::WebContents* WebRtcTestBase::OpenTestPageAndGetUserMediaInNewTab(
    const std::string& test_page) const {
  return OpenPageAndGetUserMediaInNewTab(
      embedded_test_server()->GetURL(test_page));
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
    content::WebContents* tab,
    const std::string& certificate_keygen_algorithm) const {
  SetupPeerconnectionWithoutLocalStream(tab, certificate_keygen_algorithm);
  EXPECT_EQ("ok-added", ExecuteJavascript("addLocalStream()", tab));
}

void WebRtcTestBase::SetupPeerconnectionWithoutLocalStream(
    content::WebContents* tab,
    const std::string& certificate_keygen_algorithm) const {
  std::string javascript = base::StringPrintf(
      "preparePeerConnection(%s)", certificate_keygen_algorithm.c_str());
  EXPECT_EQ("ok-peerconnection-created", ExecuteJavascript(javascript, tab));
}

void WebRtcTestBase::SetupPeerconnectionWithCertificateAndLocalStream(
    content::WebContents* tab,
    const std::string& certificate) const {
  SetupPeerconnectionWithCertificateWithoutLocalStream(tab, certificate);
  EXPECT_EQ("ok-added", ExecuteJavascript("addLocalStream()", tab));
}

void WebRtcTestBase::SetupPeerconnectionWithCertificateWithoutLocalStream(
    content::WebContents* tab,
    const std::string& certificate) const {
  std::string javascript = base::StringPrintf(
      "preparePeerConnectionWithCertificate(%s)", certificate.c_str());
  EXPECT_EQ("ok-peerconnection-created", ExecuteJavascript(javascript, tab));
}

std::string WebRtcTestBase::CreateLocalOffer(
      content::WebContents* from_tab,
      std::string default_video_codec) const {
  if (default_video_codec.empty())
    default_video_codec = "null";
  else
    default_video_codec = "'" + default_video_codec + "'";
  std::string javascript = base::StringPrintf(
      "createLocalOffer({}, %s)", default_video_codec.c_str());
  std::string response = ExecuteJavascript(javascript, from_tab);
  EXPECT_EQ("ok-", response.substr(0, 3)) << "Failed to create local offer: "
      << response;

  std::string local_offer = response.substr(3);
  return local_offer;
}

std::string WebRtcTestBase::CreateAnswer(
    std::string local_offer,
    content::WebContents* to_tab,
    std::string default_video_codec) const {
  std::string javascript =
      base::StringPrintf("receiveOfferFromPeer('%s', {})", local_offer.c_str());
  std::string response = ExecuteJavascript(javascript, to_tab);
  EXPECT_EQ("ok-", response.substr(0, 3))
      << "Receiving peer failed to receive offer and create answer: "
      << response;

  std::string answer = response.substr(3);

  if (!default_video_codec.empty()) {
    response = ExecuteJavascript(
        base::StringPrintf("verifyDefaultVideoCodec('%s', '%s')",
                           answer.c_str(), default_video_codec.c_str()),
        to_tab);
    EXPECT_EQ("ok-", response.substr(0, 3))
        << "Receiving peer failed to verify default codec: "
        << response;
  }

  return answer;
}

void WebRtcTestBase::ReceiveAnswer(const std::string& answer,
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
                                   content::WebContents* to_tab,
                                   const std::string& video_codec) const {
  std::string local_offer = CreateLocalOffer(from_tab, video_codec);
  std::string answer = CreateAnswer(local_offer, to_tab, video_codec);
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

bool WebRtcTestBase::WaitForVideoToPlay(
    content::WebContents* tab_contents) const {
  bool is_video_playing = test::PollingWaitUntil(
      "isVideoPlaying()", "video-playing", tab_contents);
  EXPECT_TRUE(is_video_playing);
  return is_video_playing;
}

std::string WebRtcTestBase::GetStreamSize(
    content::WebContents* tab_contents,
    const std::string& video_element) const {
  std::string javascript =
      base::StringPrintf("getStreamSize('%s')", video_element.c_str());
  std::string result = ExecuteJavascript(javascript, tab_contents);
  EXPECT_TRUE(base::StartsWith(result, "ok-", base::CompareCase::SENSITIVE));
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

void WebRtcTestBase::OpenDatabase(content::WebContents* tab) const {
  std::string response = ExecuteJavascript("openDatabase()", tab);
  EXPECT_EQ("ok-database-opened", response) << "Failed to open database: "
      << response;
}

void WebRtcTestBase::CloseDatabase(content::WebContents* tab) const {
  std::string response = ExecuteJavascript("closeDatabase()", tab);
  EXPECT_EQ("ok-database-closed", response) << "Failed to close database: "
      << response;
}

void WebRtcTestBase::DeleteDatabase(content::WebContents* tab) const {
  std::string response = ExecuteJavascript("deleteDatabase()", tab);
  EXPECT_EQ("ok-database-deleted", response) << "Failed to delete database: "
      << response;
}

void WebRtcTestBase::GenerateAndCloneCertificate(
    content::WebContents* tab, const std::string& keygen_algorithm) const {
  std::string javascript = base::StringPrintf(
      "generateAndCloneCertificate(%s)", keygen_algorithm.c_str());
  std::string response = ExecuteJavascript(javascript, tab);
  EXPECT_EQ("ok-generated-and-cloned", response) << "Failed to generate and "
      "clone certificate: " << response;
}
