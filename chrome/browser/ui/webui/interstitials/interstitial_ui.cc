// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interstitials/interstitial_ui.h"

#include "base/atomic_sequence_num.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ssl/bad_clock_blocking_page.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/common/features.h"
#include "chrome/common/url_constants.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/supervised_user_error_page/supervised_user_error_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_info.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/ssl/captive_portal_blocking_page.h"
#endif

namespace {

// NSS requires that serial numbers be unique even for the same issuer;
// as all fake certificates will contain the same issuer name, it's
// necessary to ensure the serial number is unique, as otherwise
// NSS will fail to parse.
base::StaticAtomicSequenceNumber g_serial_number;

scoped_refptr<net::X509Certificate> CreateFakeCert() {
  std::unique_ptr<crypto::RSAPrivateKey> unused_key;
  std::string cert_der;
  if (!net::x509_util::CreateKeyAndSelfSignedCert(
          "CN=Error", static_cast<uint32_t>(g_serial_number.GetNext()),
          base::Time::Now() - base::TimeDelta::FromMinutes(5),
          base::Time::Now() + base::TimeDelta::FromMinutes(5), &unused_key,
          &cert_der)) {
    return nullptr;
  }

  return net::X509Certificate::CreateFromBytes(cert_der.data(),
                                               cert_der.size());
}

// Implementation of chrome://interstitials demonstration pages. This code is
// not used in displaying any real interstitials.
class InterstitialHTMLSource : public content::URLDataSource {
 public:
  explicit InterstitialHTMLSource(Profile* profile) : profile_(profile) {}
  ~InterstitialHTMLSource() override {}

  // content::URLDataSource:
  std::string GetMimeType(const std::string& mime_type) const override;
  std::string GetSource() const override;
  std::string GetContentSecurityPolicyScriptSrc() const override;
  std::string GetContentSecurityPolicyStyleSrc() const override;
  std::string GetContentSecurityPolicyImgSrc() const override;
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialHTMLSource);
};

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
// Provides fake connection information to the captive portal blocking page so
// that both Wi-Fi and non Wi-Fi blocking pages can be displayed.
class CaptivePortalBlockingPageWithNetInfo : public CaptivePortalBlockingPage {
 public:
  CaptivePortalBlockingPageWithNetInfo(
      content::WebContents* web_contents,
      const GURL& request_url,
      const GURL& login_url,
      const net::SSLInfo& ssl_info,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback,
      bool is_wifi,
      const std::string& wifi_ssid)
      : CaptivePortalBlockingPage(web_contents,
                                  request_url,
                                  login_url,
                                  nullptr,
                                  ssl_info,
                                  callback),
        is_wifi_(is_wifi),
        wifi_ssid_(wifi_ssid) {}

 private:
  // CaptivePortalBlockingPage methods:
  bool IsWifiConnection() const override { return is_wifi_; }
  std::string GetWiFiSSID() const override { return wifi_ssid_; }

  const bool is_wifi_;
  const std::string wifi_ssid_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalBlockingPageWithNetInfo);
};
#endif

SSLBlockingPage* CreateSSLBlockingPage(content::WebContents* web_contents) {
  // Random parameters for SSL blocking page.
  int cert_error = net::ERR_CERT_CONTAINS_ERRORS;
  GURL request_url("https://example.com");
  bool overridable = false;
  bool strict_enforcement = false;
  base::Time time_triggered_ = base::Time::NowFromSystemTime();
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "url",
                                 &url_param)) {
    if (GURL(url_param).is_valid())
      request_url = GURL(url_param);
  }
  std::string overridable_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "overridable",
                                 &overridable_param)) {
    overridable = overridable_param == "1";
  }
  std::string strict_enforcement_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "strict_enforcement",
                                 &strict_enforcement_param)) {
    strict_enforcement = strict_enforcement_param == "1";
  }
  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  // This delegate doesn't create an interstitial.
  int options_mask = 0;
  if (overridable)
    options_mask |= security_interstitials::SSLErrorUI::SOFT_OVERRIDE_ENABLED;
  if (strict_enforcement)
    options_mask |= security_interstitials::SSLErrorUI::STRICT_ENFORCEMENT;
  return SSLBlockingPage::Create(
      web_contents, cert_error, ssl_info, request_url, options_mask,
      time_triggered_, nullptr,
      base::Callback<void(content::CertificateRequestResultType)>());
}

BadClockBlockingPage* CreateBadClockBlockingPage(
    content::WebContents* web_contents) {
  // Set up a fake clock error.
  int cert_error = net::ERR_CERT_DATE_INVALID;
  GURL request_url("https://example.com");
  bool overridable = false;
  bool strict_enforcement = false;
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "url", &url_param) &&
      GURL(url_param).is_valid()) {
    request_url = GURL(url_param);
  }
  std::string overridable_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "overridable",
                                 &overridable_param)) {
    overridable = overridable_param == "1";
  }
  std::string strict_enforcement_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "strict_enforcement",
                                 &strict_enforcement_param)) {
    strict_enforcement = strict_enforcement_param == "1";
  }

  // Determine whether to change the clock to be ahead or behind.
  std::string clock_manipulation_param;
  ssl_errors::ClockState clock_state = ssl_errors::CLOCK_STATE_PAST;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "clock_manipulation",
                                 &clock_manipulation_param)) {
    int time_offset;
    if (base::StringToInt(clock_manipulation_param, &time_offset)) {
      clock_state = time_offset > 0 ? ssl_errors::CLOCK_STATE_FUTURE
                                    : ssl_errors::CLOCK_STATE_PAST;
    }
  }

  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  // This delegate doesn't create an interstitial.
  int options_mask = 0;
  if (overridable)
    options_mask |= security_interstitials::SSLErrorUI::SOFT_OVERRIDE_ENABLED;
  if (strict_enforcement)
    options_mask |= security_interstitials::SSLErrorUI::STRICT_ENFORCEMENT;
  return new BadClockBlockingPage(
      web_contents, cert_error, ssl_info, request_url, base::Time::Now(),
      clock_state, nullptr,
      base::Callback<void(content::CertificateRequestResultType)>());
}

safe_browsing::SafeBrowsingBlockingPage* CreateSafeBrowsingBlockingPage(
    content::WebContents* web_contents) {
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  GURL request_url("http://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "url",
                                 &url_param)) {
    if (GURL(url_param).is_valid())
      request_url = GURL(url_param);
  }
  GURL main_frame_url(request_url);
  // TODO(mattm): add flag to change main_frame_url or add dedicated flag to
  // test subresource interstitials.
  std::string type_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "type",
                                 &type_param)) {
    // TODO(mattm): add param for SB_THREAT_TYPE_URL_UNWANTED.
    if (type_param == "malware") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
    } else if (type_param == "phishing") {
      threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
    } else if (type_param == "clientside_malware") {
      threat_type = safe_browsing::SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL;
    } else if (type_param == "clientside_phishing") {
      threat_type = safe_browsing::SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL;
    }
  }
  safe_browsing::SafeBrowsingBlockingPage::UnsafeResource resource;
  resource.url = request_url;
  resource.is_subresource = request_url != main_frame_url;
  resource.is_subframe = false;
  resource.threat_type = threat_type;
  resource.web_contents_getter =
      security_interstitials::UnsafeResource::
          GetWebContentsGetter(web_contents->GetRenderProcessHost()->GetID(),
                               web_contents->GetMainFrame()->GetRoutingID());
  resource.threat_source = safe_browsing::ThreatSource::LOCAL_PVER3;

  // Normally safebrowsing interstitial types which block the main page load
  // (SB_THREAT_TYPE_URL_MALWARE, SB_THREAT_TYPE_URL_PHISHING, and
  // SB_THREAT_TYPE_URL_UNWANTED on main-frame loads) would expect there to be a
  // pending navigation when the SafeBrowsingBlockingPage is created. This demo
  // creates a SafeBrowsingBlockingPage but does not actually show a real
  // interstitial. Instead it extracts the html and displays it manually, so the
  // parts which depend on the NavigationEntry are not hit.
  return safe_browsing::SafeBrowsingBlockingPage::CreateBlockingPage(
      g_browser_process->safe_browsing_service()->ui_manager().get(),
      web_contents, main_frame_url, resource);
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
CaptivePortalBlockingPage* CreateCaptivePortalBlockingPage(
    content::WebContents* web_contents) {
  bool is_wifi_connection = false;
  GURL landing_url("https://captive.portal/login");
  GURL request_url("https://google.com");
  // Not initialized to a default value, since non-empty wifi_ssid is
  // considered a wifi connection, even if is_wifi_connection is false.
  std::string wifi_ssid;

  std::string request_url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "url",
                                 &request_url_param)) {
    if (GURL(request_url_param).is_valid())
      request_url = GURL(request_url_param);
  }
  std::string landing_url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "landing_page",
                                 &landing_url_param)) {
    if (GURL(landing_url_param).is_valid())
      landing_url = GURL(landing_url_param);
  }
  std::string wifi_connection_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "is_wifi",
                                 &wifi_connection_param)) {
    is_wifi_connection = wifi_connection_param == "1";
  }
  std::string wifi_ssid_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "wifi_name",
                                 &wifi_ssid_param)) {
    wifi_ssid = wifi_ssid_param;
  }
  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();
  CaptivePortalBlockingPage* blocking_page =
      new CaptivePortalBlockingPageWithNetInfo(
          web_contents, request_url, landing_url, ssl_info,
          base::Callback<void(content::CertificateRequestResultType)>(),
          is_wifi_connection, wifi_ssid);
  return blocking_page;
}
#endif

} //  namespace

InterstitialUI::InterstitialUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, new InterstitialHTMLSource(profile));
}

InterstitialUI::~InterstitialUI() {
}

// InterstitialHTMLSource

std::string InterstitialHTMLSource::GetMimeType(
    const std::string& mime_type) const {
  return "text/html";
}

std::string InterstitialHTMLSource::GetSource() const {
  return chrome::kChromeUIInterstitialHost;
}

std::string InterstitialHTMLSource::GetContentSecurityPolicyScriptSrc() const {
  // 'unsafe-inline' is added to script-src.
  return "script-src chrome://resources 'self' 'unsafe-eval' 'unsafe-inline';";
}

std::string InterstitialHTMLSource::GetContentSecurityPolicyStyleSrc() const {
  return "style-src 'self' 'unsafe-inline';";
}

std::string InterstitialHTMLSource::GetContentSecurityPolicyImgSrc() const {
  return "img-src data:;";
}

void InterstitialHTMLSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  content::WebContents* web_contents = wc_getter.Run();
  if (!web_contents) {
    // When browser-side navigation is enabled, web_contents can be null if
    // the tab is closing. Nothing to do in this case.
    return;
  }
  std::unique_ptr<content::InterstitialPageDelegate> interstitial_delegate;
  if (base::StartsWith(path, "ssl", base::CompareCase::SENSITIVE)) {
    interstitial_delegate.reset(CreateSSLBlockingPage(web_contents));
  } else if (base::StartsWith(path, "safebrowsing",
                              base::CompareCase::SENSITIVE)) {
    interstitial_delegate.reset(CreateSafeBrowsingBlockingPage(web_contents));
  } else if (base::StartsWith(path, "clock", base::CompareCase::SENSITIVE)) {
    interstitial_delegate.reset(CreateBadClockBlockingPage(web_contents));
  }
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  else if (base::StartsWith(path, "captiveportal",
                            base::CompareCase::SENSITIVE))
  {
    interstitial_delegate.reset(CreateCaptivePortalBlockingPage(web_contents));
  }
#endif
  std::string html;
  if (base::StartsWith(path, "supervised_user", base::CompareCase::SENSITIVE)) {
    html = SupervisedUserInterstitial::GetHTMLContents(
        profile_, profile_->IsChild()
                      ? supervised_user_error_page::ASYNC_CHECKER
                      : supervised_user_error_page::MANUAL);
  } else if (interstitial_delegate.get()) {
    html = interstitial_delegate.get()->GetHTMLContents();
  } else {
    html = ResourceBundle::GetSharedInstance()
                          .GetRawDataResource(IDR_SECURITY_INTERSTITIAL_UI_HTML)
                          .as_string();
  }
  scoped_refptr<base::RefCountedString> html_bytes = new base::RefCountedString;
  html_bytes->data().assign(html.begin(), html.end());
  callback.Run(html_bytes.get());
}
