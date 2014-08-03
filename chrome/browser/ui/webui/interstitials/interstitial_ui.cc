// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interstitials/interstitial_ui.h"

#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"

namespace {

class InterstitialHTMLSource : public content::URLDataSource {
 public:
  InterstitialHTMLSource(Profile* profile,
                         content::WebContents* web_contents);
  virtual ~InterstitialHTMLSource();

  // content::URLDataSource:
  virtual std::string GetMimeType(const std::string& mime_type) const OVERRIDE;
  virtual std::string GetSource() const OVERRIDE;
  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;

 private:
  Profile* profile_;
  content::WebContents* web_contents_;
  DISALLOW_COPY_AND_ASSIGN(InterstitialHTMLSource);
};

SSLBlockingPage* CreateSSLBlockingPage(content::WebContents* web_contents) {
  // Random parameters for SSL blocking page.
  int cert_error = net::ERR_CERT_CONTAINS_ERRORS;
  GURL request_url("https://example.com");
  bool overridable = false;
  bool strict_enforcement = false;
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
  ssl_info.cert = new net::X509Certificate(
      request_url.host(), "CA", base::Time::Max(), base::Time::Max());
  // This delegate doesn't create an interstitial.
  return new SSLBlockingPage(web_contents, cert_error, ssl_info,
                             request_url, overridable,
                             strict_enforcement,
                             base::Callback<void(bool)>());
}

SafeBrowsingBlockingPage* CreateSafeBrowsingBlockingPage(
    content::WebContents* web_contents) {
  SBThreatType threat_type = SB_THREAT_TYPE_URL_MALWARE;
  GURL request_url("http://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "url",
                                 &url_param)) {
    if (GURL(url_param).is_valid())
      request_url = GURL(url_param);
  }
  std::string type_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "type",
                                 &type_param)) {
    if (type_param == "malware") {
      threat_type =  SB_THREAT_TYPE_URL_MALWARE;
    } else if (type_param == "phishing") {
      threat_type = SB_THREAT_TYPE_URL_PHISHING;
    } else if (type_param == "clientside_malware") {
      threat_type = SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL;
    } else if (type_param == "clientside_phishing") {
      threat_type = SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL;
      // Interstitials for client side phishing urls load after the page loads
      // (see SafeBrowsingBlockingPage::IsMainPageLoadBlocked), so there should
      // either be a new navigation entry, or there shouldn't be any pending
      // entries. Clear any pending navigation entries.
      content::NavigationController* controller =
          &web_contents->GetController();
      controller->DiscardNonCommittedEntries();
    }
  }
  SafeBrowsingBlockingPage::UnsafeResource resource;
  resource.url = request_url;
  resource.threat_type =  threat_type;
  // Create a blocking page without showing the interstitial.
  return SafeBrowsingBlockingPage::CreateBlockingPage(
      g_browser_process->safe_browsing_service()->ui_manager(),
      web_contents,
      resource);
}

} //  namespace

InterstitialUI::InterstitialUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  scoped_ptr<InterstitialHTMLSource> html_source(
      new InterstitialHTMLSource(profile->GetOriginalProfile(),
                                 web_ui->GetWebContents()));
  content::URLDataSource::Add(profile, html_source.release());
}

InterstitialUI::~InterstitialUI() {
}

// InterstitialHTMLSource

InterstitialHTMLSource::InterstitialHTMLSource(
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents) {
}

InterstitialHTMLSource::~InterstitialHTMLSource() {
}

std::string InterstitialHTMLSource::GetMimeType(
    const std::string& mime_type) const {
  return "text/html";
}

std::string InterstitialHTMLSource::GetSource() const {
  return chrome::kChromeUIInterstitialHost;
}

bool InterstitialHTMLSource::ShouldAddContentSecurityPolicy()
    const {
  return false;
}

void InterstitialHTMLSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  scoped_ptr<content::InterstitialPageDelegate> interstitial_delegate;
  if (StartsWithASCII(path, "ssl", true)) {
    interstitial_delegate.reset(CreateSSLBlockingPage(web_contents_));
  } else if (StartsWithASCII(path, "safebrowsing", true)) {
    interstitial_delegate.reset(CreateSafeBrowsingBlockingPage(web_contents_));
  }

  std::string html;
  if (interstitial_delegate.get()) {
    html = interstitial_delegate.get()->GetHTMLContents();
  } else {
    html = "<html><head><title>Interstitials</title></head>"
           "<body><h2>Choose an interstitial<h2>"
           "<h3>SSL</h3>"
           "<a href='ssl'>example.com</a><br>"
           "<a href='ssl?url=https://google.com'>SSL (google.com)</a><br>"
           "<a href='ssl?overridable=1&strict_enforcement=0'>"
           "    example.com (Overridable)</a>"
           "<br><br>"
           "<h3>SafeBrowsing</h3>"
           "<a href='safebrowsing?type=malware'>Malware</a><br>"
           "<a href='safebrowsing?type=phishing'>Phishing</a><br>"
           "<a href='safebrowsing?type=clientside_malware'>"
           "    Client Side Malware</a><br>"
           "<a href='safebrowsing?type=clientside_phishing'>"
           "    Client Side Phishing</a><br>"
           "</body></html>";
  }
  scoped_refptr<base::RefCountedString> html_bytes = new base::RefCountedString;
  html_bytes->data().assign(html.begin(), html.end());
  callback.Run(html_bytes.get());
}
