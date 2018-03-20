// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/views/scoped_macviews_browser_mode.h"
#include "components/security_state/core/security_state.h"
#include "components/toolbar/toolbar_field_trial.h"
#include "components/toolbar/toolbar_model_impl.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/page_zoom.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/cert/ct_policy_status.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_filter.h"
#include "services/network/public/cpp/features.h"

class LocationBarViewBrowserTest : public InProcessBrowserTest {
 public:
  LocationBarViewBrowserTest() = default;

  LocationBarView* GetLocationBarView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetLocationBarView();
  }

 private:
  test::ScopedMacViewsBrowserMode views_mode_{true};
  DISALLOW_COPY_AND_ASSIGN(LocationBarViewBrowserTest);
};

// Ensure the location bar decoration is added when zooming, and is removed when
// the bubble is closed, but only if zoom was reset.
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, LocationBarDecoration) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  ZoomView* zoom_view = GetLocationBarView()->zoom_view();

  ASSERT_TRUE(zoom_view);
  EXPECT_FALSE(zoom_view->visible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // Altering zoom should display a bubble. Note ZoomBubbleView closes
  // asynchronously, so precede checks with a run loop flush.
  zoom_controller->SetZoomLevel(content::ZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->visible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Close the bubble at other than 100% zoom. Icon should remain visible.
  ZoomBubbleView::CloseCurrentBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->visible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // Show the bubble again.
  zoom_controller->SetZoomLevel(content::ZoomFactorToZoomLevel(2.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->visible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Remains visible at 100% until the bubble is closed.
  zoom_controller->SetZoomLevel(content::ZoomFactorToZoomLevel(1.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->visible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Closing at 100% hides the icon.
  ZoomBubbleView::CloseCurrentBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(zoom_view->visible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
}

// Ensure that location bar bubbles close when the webcontents hides.
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, BubblesCloseOnHide) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  ZoomView* zoom_view = GetLocationBarView()->zoom_view();

  ASSERT_TRUE(zoom_view);
  EXPECT_FALSE(zoom_view->visible());

  zoom_controller->SetZoomLevel(content::ZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->visible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  chrome::NewTab(browser());
  chrome::SelectNextTab(browser());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
}

// After AddUrlHandler() is called, requests to this hostname will be mocked
// and use specified certificate validation results. This allows tests to mock
// Extended Validation (EV) certificate connections.
const char kMockSecureHostname[] = "example-secure.test";
const GURL kMockSecureURL = GURL("https://example-secure.test");

// A URLRequestMockHTTPJob which mocks a TLS connection with a certificate
// verification result specified in |cert_status|. Additionally sets the
// |ct_policy_compliance| so that the requirements for EV certificates are met.
class TestHttpsURLRequestJob : public net::URLRequestMockHTTPJob {
 public:
  TestHttpsURLRequestJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const base::FilePath& file_path,
                         scoped_refptr<net::X509Certificate> cert,
                         net::CertStatus cert_status)
      : net::URLRequestMockHTTPJob(request, network_delegate, file_path),
        cert_(std::move(cert)),
        cert_status_(cert_status) {}

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    net::URLRequestMockHTTPJob::GetResponseInfo(info);
    info->ssl_info.cert = cert_;
    info->ssl_info.cert_status = cert_status_;
    info->ssl_info.ct_policy_compliance =
        net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
  }

 protected:
  ~TestHttpsURLRequestJob() override {}

 private:
  const scoped_refptr<net::X509Certificate> cert_;
  const net::CertStatus cert_status_;

  DISALLOW_COPY_AND_ASSIGN(TestHttpsURLRequestJob);
};

// A URLRequestInterceptor that handles requests with TestHttpsURLRequestJob
// jobs.
class TestHttpsInterceptor : public net::URLRequestInterceptor {
 public:
  TestHttpsInterceptor(const base::FilePath& base_path,
                       scoped_refptr<net::X509Certificate> cert,
                       net::CertStatus cert_status)
      : base_path_(base_path),
        cert_(std::move(cert)),
        cert_status_(cert_status) {}
  ~TestHttpsInterceptor() override {}

  // net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new TestHttpsURLRequestJob(request, network_delegate, base_path_,
                                      cert_, cert_status_);
  }

 private:
  const base::FilePath base_path_;
  const scoped_refptr<net::X509Certificate> cert_;
  const net::CertStatus cert_status_;

  DISALLOW_COPY_AND_ASSIGN(TestHttpsInterceptor);
};

// Installs a handler to serve HTTPS requests to |kMockSecureHostname| with
// connections using |cert| that have |cert_status| as their certificate
// verification results.
void AddUrlHandler(const base::FilePath& base_path,
                   scoped_refptr<net::X509Certificate> cert,
                   net::CertStatus cert_status) {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor(
      "https", kMockSecureHostname,
      std::unique_ptr<net::URLRequestInterceptor>(
          new TestHttpsInterceptor(base_path, cert, cert_status)));
}

// Resets the URL handler. Only one handler can be set at a time.
void RemoveUrlHandler() {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->RemoveHostnameHandler("https", kMockSecureHostname);
}

class SecurityIndicatorTest : public InProcessBrowserTest {
 public:
  SecurityIndicatorTest() : InProcessBrowserTest(), cert_(nullptr) {}

  void SetUpInProcessBrowserTestFixture() override {
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(cert_);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  LocationBarView* GetLocationBarView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetLocationBarView();
  }

  void SetUpInterceptor(net::CertStatus cert_status) {
    // TODO(crbug.com/821557): Remove the non network service code path once
    // the URLLoader intercepts frame requests.
    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
          base::BindRepeating(&SecurityIndicatorTest::InterceptURLLoad,
                              base::Unretained(this), cert_status));
    } else {
      base::FilePath serve_file;
      PathService::Get(chrome::DIR_TEST_DATA, &serve_file);
      serve_file = serve_file.Append(FILE_PATH_LITERAL("title1.html"));
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(&AddUrlHandler, serve_file, cert_, cert_status));
    }
  }

  void ResetInterceptor() {
    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      url_loader_interceptor_.reset();
    } else {
      content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                       base::BindOnce(&RemoveUrlHandler));
    }
  }

  bool InterceptURLLoad(net::CertStatus cert_status,
                        content::URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url.host() != kMockSecureHostname)
      return false;
    net::SSLInfo ssl_info;
    ssl_info.cert = cert_;
    ssl_info.cert_status = cert_status;
    ssl_info.ct_policy_compliance =
        net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
    network::ResourceResponseHead resource_response;
    resource_response.mime_type = "text/html";
    params->client->OnReceiveResponse(resource_response, ssl_info,
                                      /*downloaded_file=*/nullptr);
    network::URLLoaderCompletionStatus completion_status;
    completion_status.ssl_info = ssl_info;
    params->client->OnComplete(completion_status);
    return true;
  }

 private:
  scoped_refptr<net::X509Certificate> cert_;
  test::ScopedMacViewsBrowserMode views_mode_{true};

  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;

  DISALLOW_COPY_AND_ASSIGN(SecurityIndicatorTest);
};

// Check that the security indicator text is correctly set for the various
// variations of the Security UI Study (https://crbug.com/803501).
IN_PROC_BROWSER_TEST_F(SecurityIndicatorTest, CheckIndicatorText) {
  const GURL kMockNonsecureURL =
      embedded_test_server()->GetURL("example.test", "/");
  const base::string16 kEvString = base::ASCIIToUTF16("Test CA [US]");
  const base::string16 kSecureString = base::ASCIIToUTF16("Secure");
  const base::string16 kEmptyString = base::string16();

  const std::string kDefaultVariation = std::string();
  const std::string kEvToSecureVariation(
      toolbar::features::kSimplifyHttpsIndicatorParameterEvToSecure);
  const std::string kSecureToLockVariation(
      toolbar::features::kSimplifyHttpsIndicatorParameterSecureToLock);
  const std::string kBothToLockVariation(
      toolbar::features::kSimplifyHttpsIndicatorParameterBothToLock);

  const struct {
    std::string feature_param;
    GURL url;
    net::CertStatus cert_status;
    security_state::SecurityLevel security_level;
    bool should_show_text;
    base::string16 indicator_text;
  } cases[]{// Default
            {kDefaultVariation, kMockSecureURL, net::CERT_STATUS_IS_EV,
             security_state::EV_SECURE, true, kEvString},
            {kDefaultVariation, kMockSecureURL, 0, security_state::SECURE, true,
             kSecureString},
            {kDefaultVariation, kMockNonsecureURL, 0, security_state::NONE,
             false, kEmptyString},
            // Variation: EV To Secure
            {kEvToSecureVariation, kMockSecureURL, net::CERT_STATUS_IS_EV,
             security_state::EV_SECURE, true, kSecureString},
            {kEvToSecureVariation, kMockSecureURL, 0, security_state::SECURE,
             true, kSecureString},
            {kEvToSecureVariation, kMockNonsecureURL, 0, security_state::NONE,
             false, kEmptyString},
            // Variation: Secure to Lock
            {kSecureToLockVariation, kMockSecureURL, net::CERT_STATUS_IS_EV,
             security_state::EV_SECURE, true, kEvString},
            {kSecureToLockVariation, kMockSecureURL, 0, security_state::SECURE,
             false, kEmptyString},
            {kSecureToLockVariation, kMockNonsecureURL, 0, security_state::NONE,
             false, kEmptyString},
            // Variation: Both to Lock
            {kBothToLockVariation, kMockSecureURL, net::CERT_STATUS_IS_EV,
             security_state::EV_SECURE, false, kEmptyString},
            {kBothToLockVariation, kMockSecureURL, 0, security_state::SECURE,
             false, kEmptyString},
            {kBothToLockVariation, kMockNonsecureURL, 0, security_state::NONE,
             false, kEmptyString}};

  security_state::SecurityInfo security_info;
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  LocationBarView* location_bar_view = GetLocationBarView();

  for (const auto& c : cases) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (c.feature_param.empty()) {
      scoped_feature_list.InitAndDisableFeature(
          toolbar::features::kSimplifyHttpsIndicator);
    } else {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          toolbar::features::kSimplifyHttpsIndicator,
          {{toolbar::features::kSimplifyHttpsIndicatorParameterName,
            c.feature_param}});
    }
    SetUpInterceptor(c.cert_status);
    ui_test_utils::NavigateToURL(browser(), c.url);
    helper->GetSecurityInfo(&security_info);
    EXPECT_EQ(c.security_level, security_info.security_level);
    EXPECT_EQ(c.should_show_text,
              location_bar_view->ShouldShowLocationIconText());
    EXPECT_EQ(c.indicator_text, location_bar_view->GetLocationIconText());
    ResetInterceptor();
  }
}
