// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/captive_portal_blocking_page.h"

#include <utility>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "components/certificate_reporting/error_reporter.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/url_formatter/url_formatter.h"
#include "components/wifi/wifi_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/ssl/ssl_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Events for UMA.
enum CaptivePortalBlockingPageEvent {
  SHOW_ALL,
  OPEN_LOGIN_PAGE,
  CAPTIVE_PORTAL_BLOCKING_PAGE_EVENT_COUNT
};

void RecordUMA(CaptivePortalBlockingPageEvent event) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.captive_portal", event,
                            CAPTIVE_PORTAL_BLOCKING_PAGE_EVENT_COUNT);
}

} // namespace

// static
const void* const CaptivePortalBlockingPage::kTypeForTesting =
    &CaptivePortalBlockingPage::kTypeForTesting;

CaptivePortalBlockingPage::CaptivePortalBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    const GURL& login_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const net::SSLInfo& ssl_info,
    const base::Callback<void(bool)>& callback)
    : SecurityInterstitialPage(web_contents, request_url),
      login_url_(login_url),
      callback_(callback) {
  DCHECK(login_url_.is_valid());

  if (ssl_cert_reporter) {
    cert_report_helper_.reset(new CertReportHelper(
        std::move(ssl_cert_reporter), web_contents, request_url, ssl_info,
        certificate_reporting::ErrorReport::INTERSTITIAL_CAPTIVE_PORTAL, false,
        nullptr));
  }

  RecordUMA(SHOW_ALL);
}

CaptivePortalBlockingPage::~CaptivePortalBlockingPage() {
}

const void* CaptivePortalBlockingPage::GetTypeForTesting() const {
  return CaptivePortalBlockingPage::kTypeForTesting;
}

bool CaptivePortalBlockingPage::IsWifiConnection() const {
  // |net::NetworkChangeNotifier::GetConnectionType| isn't accurate on Linux
  // and Windows. See https://crbug.com/160537 for details.
  // TODO(meacer): Add heuristics to get a more accurate connection type on
  //               these platforms.
  return net::NetworkChangeNotifier::GetConnectionType() ==
         net::NetworkChangeNotifier::CONNECTION_WIFI;
}

std::string CaptivePortalBlockingPage::GetWiFiSSID() const {
  // On Windows and Mac, |WiFiService| provides an easy to use API to get the
  // currently associated WiFi access point. |WiFiService| isn't available on
  // Linux so |net::GetWifiSSID| is used instead.
  std::string ssid;
#if defined(OS_WIN) || defined(OS_MACOSX)
  std::unique_ptr<wifi::WiFiService> wifi_service(wifi::WiFiService::Create());
  wifi_service->Initialize(nullptr);
  std::string error;
  wifi_service->GetConnectedNetworkSSID(&ssid, &error);
  if (!error.empty())
    return std::string();
#elif defined(OS_LINUX)
  ssid = net::GetWifiSSID();
#endif
  // TODO(meacer): Handle non UTF8 SSIDs.
  if (!base::IsStringUTF8(ssid))
    return std::string();
  return ssid;
}

bool CaptivePortalBlockingPage::ShouldCreateNewNavigation() const {
  // Captive portal interstitials always create new navigation entries, as
  // opposed to SafeBrowsing subresource interstitials which just block access
  // to the current page and don't create a new entry.
  return true;
}

void CaptivePortalBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetString("iconClass", "icon-offline");
  load_time_data->SetString("type", "CAPTIVE_PORTAL");
  load_time_data->SetBoolean("overridable", false);

  // |IsWifiConnection| isn't accurate on some platforms, so always try to get
  // the Wi-Fi SSID even if |IsWifiConnection| is false.
  std::string wifi_ssid = GetWiFiSSID();
  bool is_wifi = !wifi_ssid.empty() || IsWifiConnection();

  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_BUTTON_OPEN_LOGIN_PAGE));

  base::string16 tab_title =
      l10n_util::GetStringUTF16(is_wifi ? IDS_CAPTIVE_PORTAL_HEADING_WIFI
                                        : IDS_CAPTIVE_PORTAL_HEADING_WIRED);
  load_time_data->SetString("tabTitle", tab_title);
  load_time_data->SetString("heading", tab_title);

  base::string16 paragraph;
  if (login_url_.spec() == captive_portal::CaptivePortalDetector::kDefaultURL) {
    // Captive portal may intercept requests without HTTP redirects, in which
    // case the login url would be the same as the captive portal detection url.
    // Don't show the login url in that case.
    if (wifi_ssid.empty()) {
      paragraph = l10n_util::GetStringUTF16(
          is_wifi ? IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIFI
                  : IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIRED);
    } else {
      paragraph = l10n_util::GetStringFUTF16(
          IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIFI_SSID,
          net::EscapeForHTML(base::UTF8ToUTF16(wifi_ssid)));
    }
  } else {
    // Portal redirection was done with HTTP redirects, so show the login URL.
    // If |languages| is empty, punycode in |login_host| will always be decoded.
    base::string16 login_host =
        url_formatter::IDNToUnicode(login_url_.host());
    if (base::i18n::IsRTL())
      base::i18n::WrapStringWithLTRFormatting(&login_host);

    if (wifi_ssid.empty()) {
      paragraph = l10n_util::GetStringFUTF16(
          is_wifi ? IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIFI
                  : IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIRED,
          login_host);
    } else {
      paragraph = l10n_util::GetStringFUTF16(
          IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIFI_SSID,
          net::EscapeForHTML(base::UTF8ToUTF16(wifi_ssid)), login_host);
    }
  }
  load_time_data->SetString("primaryParagraph", paragraph);
  // Explicitly specify other expected fields to empty.
  load_time_data->SetString("openDetails", base::string16());
  load_time_data->SetString("closeDetails", base::string16());
  load_time_data->SetString("explanationParagraph", base::string16());
  load_time_data->SetString("finalParagraph", base::string16());

  if (cert_report_helper_)
    cert_report_helper_->PopulateExtendedReportingOption(load_time_data);
  else
    load_time_data->SetBoolean(security_interstitials::kDisplayCheckBox, false);
}

void CaptivePortalBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }
  int command_num = 0;
  bool command_is_num = base::StringToInt(command, &command_num);
  DCHECK(command_is_num) << command;
  // Any command other than "open the login page" is ignored.
  if (command_num == security_interstitials::CMD_OPEN_LOGIN) {
    RecordUMA(OPEN_LOGIN_PAGE);
    CaptivePortalTabHelper::OpenLoginTabForWebContents(web_contents(), true);
  }
}

void CaptivePortalBlockingPage::OnProceed() {
  if (cert_report_helper_) {
    // Finish collecting information about invalid certificates, if the
    // user opted in to.
    cert_report_helper_->FinishCertCollection(
        certificate_reporting::ErrorReport::USER_PROCEEDED);
  }
}

void CaptivePortalBlockingPage::OnDontProceed() {
  if (cert_report_helper_) {
    // Finish collecting information about invalid certificates, if the
    // user opted in to.
    cert_report_helper_->FinishCertCollection(
        certificate_reporting::ErrorReport::USER_DID_NOT_PROCEED);
  }

  // Need to explicity deny the certificate via the callback, otherwise memory
  // is leaked.
  if (!callback_.is_null()) {
    callback_.Run(false);
    callback_.Reset();
  }
}
