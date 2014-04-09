// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper_core.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "chrome/common/net/net_error_info.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebURLError.h"

using blink::WebURLError;
using chrome_common_net::DnsProbeStatus;
using chrome_common_net::DnsProbeStatusToString;

const char kFailedUrl[] = "http://failed/";
const char kFailedHttpsUrl[] = "https://failed/";
const char kLinkDoctorUrl[] = "http://link.doctor/";
const char kLinkDoctorBody[] = "Link Doctor Body";

// Creates a string from an error that is used as a mock locally generated
// error page for that error.
std::string ErrorToString(const WebURLError& error, bool is_failed_post) {
  return base::StringPrintf("(%s, %s, %i, %s)",
                            error.unreachableURL.string().utf8().c_str(),
                            error.domain.utf8().c_str(), error.reason,
                            is_failed_post ? "POST" : "NOT POST");
}

WebURLError ProbeError(DnsProbeStatus status) {
  WebURLError error;
  error.unreachableURL = GURL(kFailedUrl);
  error.domain = blink::WebString::fromUTF8(
      chrome_common_net::kDnsProbeErrorDomain);
  error.reason = status;
  return error;
}

WebURLError NetError(net::Error net_error) {
  WebURLError error;
  error.unreachableURL = GURL(kFailedUrl);
  error.domain = blink::WebString::fromUTF8(net::kErrorDomain);
  error.reason = net_error;
  return error;
}

WebURLError HttpError(int status_code) {
  WebURLError error;
  error.unreachableURL = GURL(kFailedUrl);
  error.domain = blink::WebString::fromUTF8("http");
  error.reason = status_code;
  return error;
}

// Convenience functions that create an error string for a non-POST request.

std::string ProbeErrorString(DnsProbeStatus status) {
  return ErrorToString(ProbeError(status), false);
}

std::string NetErrorString(net::Error net_error) {
  return ErrorToString(NetError(net_error), false);
}

class NetErrorHelperCoreTest : public testing::Test,
                               public NetErrorHelperCore::Delegate {
 public:
  NetErrorHelperCoreTest() : timer_(new base::MockTimer(false, false)),
                             core_(this),
                             update_count_(0),
                             error_html_update_count_(0),
                             reload_count_(0),
                             enable_stale_load_bindings_count_(0) {
    core_.set_auto_reload_enabled(false);
    core_.set_timer_for_testing(scoped_ptr<base::Timer>(timer_));
  }

  virtual ~NetErrorHelperCoreTest() {
    // No test finishes while an error page is being fetched.
    EXPECT_FALSE(is_url_being_fetched());
  }

  NetErrorHelperCore& core() { return core_; }

  const GURL& url_being_fetched() const { return url_being_fetched_; }
  bool is_url_being_fetched() const { return !url_being_fetched_.is_empty(); }

  int reload_count() const {
    return reload_count_;
  }

  int enable_stale_load_bindings_count() const {
    return enable_stale_load_bindings_count_;
  }

  const std::string& last_update_string() const { return last_update_string_; }
  int update_count() const { return update_count_;  }

  const std::string& last_error_html() const { return last_error_html_; }
  int error_html_update_count() const { return error_html_update_count_; }

  void LinkDoctorLoadSuccess() {
    LinkDoctorLoadFinished(kLinkDoctorBody);
  }

  void LinkDoctorLoadFailure() {
    LinkDoctorLoadFinished("");
  }

  base::MockTimer* timer() { return timer_; }

  void DoErrorLoad(net::Error error) {
    core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                       NetErrorHelperCore::NON_ERROR_PAGE);
    std::string html;
    core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                        NetError(error), false, &html);
    EXPECT_FALSE(html.empty());
    EXPECT_EQ(NetErrorString(error), html);

    core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                       NetErrorHelperCore::ERROR_PAGE);
    core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
    core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  }

  void DoSuccessLoad() {
    core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                       NetErrorHelperCore::NON_ERROR_PAGE);
    core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
    core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  }

  void DoDnsProbe(chrome_common_net::DnsProbeStatus final_status) {
    core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
    core().OnNetErrorInfo(final_status);
  }

 private:
  void LinkDoctorLoadFinished(const std::string& result) {
    url_being_fetched_ = GURL();
    core().OnAlternateErrorPageFetched(result);
  }

  // NetErrorHelperCore::Delegate implementation:
  virtual void GenerateLocalizedErrorPage(const WebURLError& error,
                                          bool is_failed_post,
                                          std::string* html) const OVERRIDE {
    *html = ErrorToString(error, is_failed_post);
  }

  virtual void LoadErrorPageInMainFrame(const std::string& html,
                                        const GURL& failed_url) OVERRIDE {
    error_html_update_count_++;
    last_error_html_ = html;
  }

  virtual void EnableStaleLoadBindings(const GURL& page_url) OVERRIDE {
    enable_stale_load_bindings_count_++;
  }

  virtual void UpdateErrorPage(const WebURLError& error,
                               bool is_failed_post) OVERRIDE {
    update_count_++;
    last_error_html_ = ErrorToString(error, is_failed_post);
  }

  virtual void FetchErrorPage(const GURL& url) OVERRIDE {
    EXPECT_TRUE(url_being_fetched_.is_empty());
    EXPECT_TRUE(url.is_valid());
    EXPECT_NE(std::string::npos, url.spec().find(kLinkDoctorUrl));

    url_being_fetched_ = url;
  }

  virtual void CancelFetchErrorPage() OVERRIDE {
    url_being_fetched_ = GURL();
  }

  virtual void ReloadPage() OVERRIDE {
    reload_count_++;
  }

  base::MockTimer* timer_;

  NetErrorHelperCore core_;

  GURL url_being_fetched_;

  // Contains the information passed to the last call to UpdateErrorPage, as a
  // string.
  std::string last_update_string_;
  // Number of times |last_update_string_| has been changed.
  int update_count_;

  // Contains the HTML set by the last call to LoadErrorPageInMainFrame.
  std::string last_error_html_;
  // Number of times |last_error_html_| has been changed.
  int error_html_update_count_;

  int reload_count_;

  int enable_stale_load_bindings_count_;
};

//------------------------------------------------------------------------------
// Basic tests that don't update the error page for probes or load the Link
// Doctor.
//------------------------------------------------------------------------------

TEST_F(NetErrorHelperCoreTest, Null) {
}

TEST_F(NetErrorHelperCoreTest, SuccessfulPageLoad) {
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

TEST_F(NetErrorHelperCoreTest, SuccessfulPageLoadWithLinkDoctor) {
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

TEST_F(NetErrorHelperCoreTest, MainFrameNonDnsError) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_CONNECTION_RESET), false, &html);
  // Should have returned a local error page.
  EXPECT_FALSE(html.empty());
  EXPECT_EQ(NetErrorString(net::ERR_CONNECTION_RESET), html);

  // Error page loads.
  EXPECT_EQ(0, enable_stale_load_bindings_count());
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
  EXPECT_EQ(1, enable_stale_load_bindings_count());
}

TEST_F(NetErrorHelperCoreTest, MainFrameNonDnsErrorWithLinkDoctor) {
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));

  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_CONNECTION_RESET), false, &html);
  // Should have returned a local error page.
  EXPECT_FALSE(html.empty());
  EXPECT_EQ(NetErrorString(net::ERR_CONNECTION_RESET), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Much like above tests, but with a bunch of spurious DNS status messages that
// should have no effect.
TEST_F(NetErrorHelperCoreTest, MainFrameNonDnsErrorSpuriousStatus) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_CONNECTION_RESET),
                      false, &html);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  // Should have returned a local error page.
  EXPECT_FALSE(html.empty());
  EXPECT_EQ(NetErrorString(net::ERR_CONNECTION_RESET),  html);

  // Error page loads.

  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

TEST_F(NetErrorHelperCoreTest, SubFrameDnsError) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::SUB_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page.
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::SUB_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::SUB_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

TEST_F(NetErrorHelperCoreTest, SubFrameDnsErrorWithLinkDoctor) {
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));

  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::SUB_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page.
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::SUB_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::SUB_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Much like above tests, but with a bunch of spurious DNS status messages that
// should have no effect.
TEST_F(NetErrorHelperCoreTest, SubFrameDnsErrorSpuriousStatus) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::SUB_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  // Should have returned a local error page.
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), html);

  // Error page loads.

  core().OnStartLoad(NetErrorHelperCore::SUB_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  core().OnCommitLoad(NetErrorHelperCore::SUB_FRAME);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  core().OnFinishLoad(NetErrorHelperCore::SUB_FRAME);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

//------------------------------------------------------------------------------
// Tests for updating the error page in response to DNS probe results.  None
// of these have the Link Doctor enabled.
//------------------------------------------------------------------------------

// Test case where the error page finishes loading before receiving any DNS
// probe messages.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbe) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_STARTED),
            last_error_html());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Any other probe updates should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Same as above, but the probe is not run.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeNotRun) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  // When the not run status arrives, the page should revert to the normal dns
  // error page.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_NOT_RUN);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());

  // Any other probe updates should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Same as above, but the probe result is inconclusive.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeInconclusive) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_STARTED),
            last_error_html());

  // When the inconclusive status arrives, the page should revert to the normal
  // dns error page.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());

  // Any other probe updates should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Same as above, but the probe result is no internet.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeNoInternet) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_STARTED),
            last_error_html());

  // When the inconclusive status arrives, the page should revert to the normal
  // dns error page.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());

  // Any other probe updates should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Same as above, but the probe result is bad config.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeBadConfig) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_STARTED),
            last_error_html());

  // When the inconclusive status arrives, the page should revert to the normal
  // dns error page.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_BAD_CONFIG);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_BAD_CONFIG),
            last_error_html());

  // Any other probe updates should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_BAD_CONFIG);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Test case where the error page finishes loading after receiving the start
// DNS probe message.
TEST_F(NetErrorHelperCoreTest, FinishedAfterStartProbe) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);

  // Nothing should be done when a probe status comes in before loading
  // finishes.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  EXPECT_EQ(0, update_count());

  // When loading finishes, however, the buffered probe status should be sent
  // to the page.
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_STARTED),
            last_error_html());

  // Should update the page again when the probe result comes in.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Any other probe updates should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_NOT_RUN);
  EXPECT_EQ(2, update_count());
}

// Test case where the error page finishes loading before receiving any DNS
// probe messages and the request is a POST.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbePost) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      true, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ErrorToString(
                ProbeError(chrome_common_net::DNS_PROBE_POSSIBLE),
                           true),
            html);

  // Error page loads.
  EXPECT_EQ(0, enable_stale_load_bindings_count());
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, enable_stale_load_bindings_count());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ErrorToString(
                ProbeError(chrome_common_net::DNS_PROBE_STARTED), true),
            last_error_html());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ErrorToString(
                ProbeError(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN),
                           true),
            last_error_html());
  EXPECT_EQ(0, error_html_update_count());
}

// Test case where the probe finishes before the page is committed.
TEST_F(NetErrorHelperCoreTest, ProbeFinishesEarly) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);

  // Nothing should be done when the probe statuses come in before loading
  // finishes.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  // When loading finishes, however, the buffered probe status should be sent
  // to the page.
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Any other probe updates should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(1, update_count());
}

// Test case where one error page loads completely before a new navigation
// results in another error page.  Probes are run for both pages.
TEST_F(NetErrorHelperCoreTest, TwoErrorsWithProbes) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe results come in.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // The process starts again.

  // Normal page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(2, update_count());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  EXPECT_EQ(3, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_STARTED),
            last_error_html());

  // The probe returns a different result this time.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(4, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());
  EXPECT_EQ(0, error_html_update_count());
}

// Test case where one error page loads completely before a new navigation
// results in another error page.  Probe results for the first probe are only
// received after the second load starts, but before it commits.
TEST_F(NetErrorHelperCoreTest, TwoErrorsWithProbesAfterSecondStarts) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // The process starts again.

  // Normal page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page starts to load.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);

  // Probe results come in, and the first page is updated.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Second page finishes loading, and is updated using the same probe result.
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(3, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Other probe results should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(3, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// Same as above, but a new page is loaded before the error page commits.
TEST_F(NetErrorHelperCoreTest, ErrorPageLoadInterrupted) {
  // Original page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and an error page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  // Probe statuses come in, but should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  // A new navigation begins while the error page is loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // And fails.
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE), html);

  // Error page finishes loading.
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe results come in.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_STARTED),
            last_error_html());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());
  EXPECT_EQ(0, error_html_update_count());
}

//------------------------------------------------------------------------------
// Link Doctor tests.
//------------------------------------------------------------------------------

// Check that the Link Doctor is not used for HTTPS URLs.
TEST_F(NetErrorHelperCoreTest, NoLinkDoctorForHttps) {
  // Original page starts loading.
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // The HTTPS page fails to load.
  std::string html;
  blink::WebURLError error = NetError(net::ERR_NAME_NOT_RESOLVED);
  error.unreachableURL = GURL(kFailedHttpsUrl);
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME, error, false, &html);

  blink::WebURLError probe_error =
      ProbeError(chrome_common_net::DNS_PROBE_POSSIBLE);
  probe_error.unreachableURL = GURL(kFailedHttpsUrl);
  EXPECT_EQ(ErrorToString(probe_error, false), html);
  EXPECT_FALSE(is_url_being_fetched());

  // The blank page loads, no error page is loaded.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(is_url_being_fetched());

  // Page is updated in response to DNS probes as normal.
  EXPECT_EQ(0, update_count());
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  blink::WebURLError final_probe_error =
      ProbeError(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  final_probe_error.unreachableURL = GURL(kFailedHttpsUrl);
  EXPECT_EQ(ErrorToString(final_probe_error, false), last_error_html());
}

// The blank page loads, then the Link Doctor request succeeds and is loaded.
// Then the probe results come in.
TEST_F(NetErrorHelperCoreTest, LinkDoctorSucceedsBeforeProbe) {
  // Original page starts loading.
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  EXPECT_TRUE(html.empty());
  EXPECT_FALSE(is_url_being_fetched());

  // The blank page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);

  // Link doctor retrieval starts when the error page finishes loading.
  EXPECT_FALSE(is_url_being_fetched());
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());

  // Link Doctor is retrieved.
  LinkDoctorLoadSuccess();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(kLinkDoctorBody, last_error_html());
  EXPECT_FALSE(is_url_being_fetched());

  // Link Doctor page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Any probe statuses should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);

  EXPECT_EQ(0, update_count());
  EXPECT_EQ(1, error_html_update_count());
}

// The blank page finishes loading, then probe results come in, and then
// the Link Doctor request succeeds.
TEST_F(NetErrorHelperCoreTest, LinkDoctorSucceedsAfterProbes) {
  // Original page starts loading.
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and a Link Doctor page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());


  // Probe statuses should be ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());

  // Link Doctor is retrieved.
  EXPECT_TRUE(is_url_being_fetched());
  LinkDoctorLoadSuccess();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(kLinkDoctorBody, last_error_html());
  EXPECT_FALSE(is_url_being_fetched());

  // Link Doctor page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(0, update_count());
}

// The Link Doctor request fails and then the error page loads for an error that
// does not trigger DNS probes.
TEST_F(NetErrorHelperCoreTest, LinkDoctorFailsLoadNoProbes) {
  // Original page starts loading.
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and a Link Doctor page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_CONNECTION_FAILED),
                      false, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Link Doctor load fails, final error page is shown.
  EXPECT_TRUE(is_url_being_fetched());
  LinkDoctorLoadFailure();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(last_error_html(), NetErrorString(net::ERR_CONNECTION_FAILED));
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());

  // Error page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // If probe statuses come in last from another page load, they should be
  // ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(1, error_html_update_count());
}

// The Link Doctor request fails and then the error page loads before probe
// results are received.
TEST_F(NetErrorHelperCoreTest, LinkDoctorFailsLoadBeforeProbe) {
  // Original page starts loading.
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and a Link Doctor page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Link Doctor load fails, probe pending page shown.
  EXPECT_TRUE(is_url_being_fetched());
  LinkDoctorLoadFailure();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(last_error_html(),
            ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE));
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());

  // Probe page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe statuses comes in, and page is updated.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_STARTED),
            last_error_html());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // The commit results in sending a second probe status, which is ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(1, error_html_update_count());
}

// The Link Doctor request fails after receiving probe results.
TEST_F(NetErrorHelperCoreTest, LinkDoctorFailsAfterProbe) {
  // Original page starts loading.
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and a Link Doctor page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Results come in, but end up being ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  // Link Doctor load fails, probe pending page shown.
  EXPECT_TRUE(is_url_being_fetched());
  LinkDoctorLoadFailure();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(last_error_html(),
            ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE));
  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());

  // Probe page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe statuses comes in, and page is updated.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());
  EXPECT_EQ(1, error_html_update_count());
}

// An error page load that would normally load the Link Doctor is interrupted
// by a new navigation before the blank page commits.
TEST_F(NetErrorHelperCoreTest, LinkDoctorInterruptedBeforeCommit) {
  // Original page starts loading.
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and a Link Doctor page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  EXPECT_TRUE(html.empty());

  // The blank page starts loading.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);

  // A new page load starts.
  EXPECT_FALSE(is_url_being_fetched());
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // A new page load interrupts the original load.
  EXPECT_FALSE(is_url_being_fetched());
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  EXPECT_FALSE(is_url_being_fetched());
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(is_url_being_fetched());
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// An error page load that would normally load the Link Doctor is interrupted
// by a new navigation before the blank page finishes loading.
TEST_F(NetErrorHelperCoreTest, LinkDoctorInterruptedBeforeLoad) {
  // Original page starts loading.
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and a Link Doctor page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  EXPECT_TRUE(html.empty());

  // The blank page starts loading and is committed.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);

  // A new page load interrupts the original load.
  EXPECT_FALSE(is_url_being_fetched());
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  EXPECT_FALSE(is_url_being_fetched());
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(is_url_being_fetched());
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  EXPECT_FALSE(is_url_being_fetched());
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(0, error_html_update_count());
}

// The Link Doctor request is cancelled due to a new navigation.  The new
// navigation fails and then loads the link doctor page again (Successfully).
TEST_F(NetErrorHelperCoreTest, LinkDoctorInterrupted) {
  // Original page starts loading.
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and a Link Doctor page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());

  // Results come in, but end up being ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  // A new load appears!
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  EXPECT_FALSE(is_url_being_fetched());

  // It fails, and a Link Doctor page is requested again once a blank page is
  // loaded.
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  EXPECT_TRUE(html.empty());
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(is_url_being_fetched());
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());

  // Link Doctor load succeeds.
  LinkDoctorLoadSuccess();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(kLinkDoctorBody, last_error_html());
  EXPECT_FALSE(is_url_being_fetched());

  // Probe statuses come in, and are ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());
}

// The Link Doctor request is cancelled due to call to Stop().  The cross
// process navigation is cancelled, and then a new load fails and tries to load
// the link doctor page again (Which fails).
TEST_F(NetErrorHelperCoreTest, LinkDoctorStopped) {
  // Original page starts loading.
  core().set_alt_error_page_url(GURL(kLinkDoctorUrl));
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and a Link Doctor page is requested.
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  EXPECT_TRUE(is_url_being_fetched());
  core().OnStop();
  EXPECT_FALSE(is_url_being_fetched());

  // Results come in, but end up being ignored.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  // Cross process navigation must have been cancelled, and a new load appears!
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);

  // It fails, and a Link Doctor page is requested again.
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_NAME_NOT_RESOLVED),
                      false, &html);
  EXPECT_TRUE(html.empty());

  // The blank page loads again.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_TRUE(is_url_being_fetched());

  // Link Doctor load fails, probe pending page shown.
  LinkDoctorLoadFailure();
  EXPECT_EQ(1, error_html_update_count());
  EXPECT_EQ(last_error_html(),
            ProbeErrorString(chrome_common_net::DNS_PROBE_POSSIBLE));
  EXPECT_FALSE(is_url_being_fetched());

  // Probe page loads.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe statuses comes in, and page is updated.
  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_STARTED);
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_STARTED),
            last_error_html());
  EXPECT_EQ(1, update_count());

  core().OnNetErrorInfo(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());
  EXPECT_EQ(1, error_html_update_count());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadDisabled) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);

  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(0, reload_count());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadSucceeds) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);

  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_EQ(0, reload_count());

  timer()->Fire();
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(1, reload_count());

  DoSuccessLoad();

  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadRetries) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);

  EXPECT_TRUE(timer()->IsRunning());
  base::TimeDelta first_delay = timer()->GetCurrentDelay();
  EXPECT_EQ(0, reload_count());

  timer()->Fire();
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(1, reload_count());

  DoErrorLoad(net::ERR_CONNECTION_RESET);

  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_GT(timer()->GetCurrentDelay(), first_delay);
}

TEST_F(NetErrorHelperCoreTest, AutoReloadStopsTimerOnStop) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(timer()->IsRunning());
  core().OnStop();
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadStopsLoadingOnStop) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_EQ(1, core().auto_reload_count());
  timer()->Fire();
  EXPECT_EQ(1, reload_count());
  core().OnStop();
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(0, core().auto_reload_count());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadStopsOnOtherLoadStart) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(timer()->IsRunning());
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(1, core().auto_reload_count());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadResetsCountOnSuccess) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  base::TimeDelta delay = timer()->GetCurrentDelay();
  EXPECT_EQ(1, core().auto_reload_count());
  timer()->Fire();
  EXPECT_EQ(1, reload_count());
  DoSuccessLoad();
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_EQ(1, core().auto_reload_count());
  EXPECT_EQ(timer()->GetCurrentDelay(), delay);
  timer()->Fire();
  EXPECT_EQ(2, reload_count());
  DoSuccessLoad();
  EXPECT_EQ(0, core().auto_reload_count());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadRestartsOnOnline) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  base::TimeDelta delay = timer()->GetCurrentDelay();
  timer()->Fire();
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_NE(delay, timer()->GetCurrentDelay());
  core().NetworkStateChanged(true);
  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_EQ(delay, timer()->GetCurrentDelay());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadDoesNotStartOnOnline) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  timer()->Fire();
  DoSuccessLoad();
  EXPECT_FALSE(timer()->IsRunning());
  core().NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadStopsOnOffline) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(timer()->IsRunning());
  core().NetworkStateChanged(false);
  EXPECT_FALSE(timer()->IsRunning());
  EXPECT_EQ(0, core().auto_reload_count());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadStopsOnOfflineThenRestartsOnOnline) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(timer()->IsRunning());
  core().NetworkStateChanged(false);
  EXPECT_FALSE(timer()->IsRunning());
  core().NetworkStateChanged(true);
  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_EQ(1, core().auto_reload_count());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadDoesNotRestartOnOnlineAfterStop) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  timer()->Fire();
  core().OnStop();
  core().NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadWithDnsProbes) {
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  DoDnsProbe(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN);
  timer()->Fire();
  EXPECT_EQ(1, reload_count());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadExponentialBackoffLevelsOff) {
  core().set_auto_reload_enabled(true);
  base::TimeDelta previous = base::TimeDelta::FromMilliseconds(0);
  const int kMaxTries = 50;
  int tries = 0;
  for (tries = 0; tries < kMaxTries; tries++) {
    DoErrorLoad(net::ERR_CONNECTION_RESET);
    EXPECT_TRUE(timer()->IsRunning());
    if (previous == timer()->GetCurrentDelay())
      break;
    previous = timer()->GetCurrentDelay();
    timer()->Fire();
  }

  EXPECT_LT(tries, kMaxTries);
}

TEST_F(NetErrorHelperCoreTest, AutoReloadSlowError) {
  core().set_auto_reload_enabled(true);
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_CONNECTION_RESET), false, &html);
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(timer()->IsRunning());
  // Start a new non-error page load.
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  EXPECT_FALSE(timer()->IsRunning());
  // Finish the error page load.
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(timer()->IsRunning());
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadOnlineSlowError) {
  core().set_auto_reload_enabled(true);
  core().NetworkStateChanged(false);
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_CONNECTION_RESET), false, &html);
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(timer()->IsRunning());
  core().NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
  core().NetworkStateChanged(false);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(timer()->IsRunning());
  core().NetworkStateChanged(true);
  EXPECT_TRUE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadOnlinePendingError) {
  core().set_auto_reload_enabled(true);
  core().NetworkStateChanged(false);
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_CONNECTION_RESET), false, &html);
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  EXPECT_FALSE(timer()->IsRunning());
  core().NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
  core().NetworkStateChanged(false);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_FALSE(timer()->IsRunning());
  core().NetworkStateChanged(true);
  EXPECT_TRUE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreTest, AutoReloadOnlinePartialErrorReplacement) {
  core().set_auto_reload_enabled(true);
  core().NetworkStateChanged(false);
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  std::string html;
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_CONNECTION_RESET), false, &html);
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  core().OnCommitLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::NON_ERROR_PAGE);
  core().GetErrorHTML(NetErrorHelperCore::MAIN_FRAME,
                      NetError(net::ERR_CONNECTION_RESET), false, &html);
  core().OnStartLoad(NetErrorHelperCore::MAIN_FRAME,
                     NetErrorHelperCore::ERROR_PAGE);
  EXPECT_FALSE(timer()->IsRunning());
  core().NetworkStateChanged(true);
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(NetErrorHelperCoreTest, ShouldSuppressErrorPage) {
  // Set up the environment to test ShouldSuppressErrorPage: auto-reload is
  // enabled, an error page is loaded, and the auto-reload callback is running.
  core().set_auto_reload_enabled(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  timer()->Fire();

  EXPECT_FALSE(core().ShouldSuppressErrorPage(NetErrorHelperCore::SUB_FRAME,
                                              GURL(kFailedUrl)));
  EXPECT_FALSE(core().ShouldSuppressErrorPage(NetErrorHelperCore::MAIN_FRAME,
                                              GURL("http://some.other.url")));
  EXPECT_TRUE(core().ShouldSuppressErrorPage(NetErrorHelperCore::MAIN_FRAME,
                                             GURL(kFailedUrl)));
}
