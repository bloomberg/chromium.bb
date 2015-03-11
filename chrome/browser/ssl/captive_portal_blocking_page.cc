// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/captive_portal_blocking_page.h"

#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "components/wifi/wifi_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#error This file must be built with ENABLE_CAPTIVE_PORTAL_DETECTION flag.
#endif

namespace {

// Events for UMA.
enum CaptivePortalBlockingPageEvent {
  SHOW_ALL,
  OPEN_LOGIN_PAGE,
  CAPTIVE_PORTAL_BLOCKING_PAGE_EVENT_COUNT
};

void RecordUMA(CaptivePortalBlockingPageEvent event) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.captive_portal",
                            event,
                            CAPTIVE_PORTAL_BLOCKING_PAGE_EVENT_COUNT);
}

class ConnectionInfoDelegate : public CaptivePortalBlockingPage::Delegate {
 public:
  ConnectionInfoDelegate() {}
  ~ConnectionInfoDelegate() override {}

  bool IsWifiConnection() const override {
    // |net::NetworkChangeNotifier::GetConnectionType| isn't accurate on Linux
    // and Windows. See https://crbug.com/160537 for details.
    // TODO(meacer): Add heuristics to get a more accurate connection type on
    //               these platforms.
    return net::NetworkChangeNotifier::GetConnectionType() ==
           net::NetworkChangeNotifier::CONNECTION_WIFI;
  }

  std::string GetWiFiSSID() const override {
    // On Windows and Mac, |WiFiService| provides an easy to use API to get the
    // currently associated WiFi access point. |WiFiService| isn't available on
    // Linux so |net::GetWifiSSID| is used instead.
    std::string ssid;
#if defined(OS_WIN) || defined(OS_MACOSX)
    scoped_ptr<wifi::WiFiService> wifi_service(wifi::WiFiService::Create());
    wifi_service->Initialize(NULL);
    std::string error;
    wifi_service->GetConnectedNetworkSSID(&ssid, &error);
    if (!error.empty())
      return "";
#elif defined(OS_LINUX)
    ssid = net::GetWifiSSID();
#endif
    // TODO(meacer): Handle non UTF8 SSIDs.
    if (!base::IsStringUTF8(ssid))
      return "";
    return ssid;
  }
};

} // namespace

// static
const void* CaptivePortalBlockingPage::kTypeForTesting =
    &CaptivePortalBlockingPage::kTypeForTesting;

CaptivePortalBlockingPage::CaptivePortalBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    const GURL& login_url,
    const base::Callback<void(bool)>& callback)
    : SecurityInterstitialPage(web_contents, request_url),
      login_url_(login_url),
      delegate_(new ConnectionInfoDelegate),
      callback_(callback) {
  DCHECK(login_url_.is_valid());
  RecordUMA(SHOW_ALL);
}

CaptivePortalBlockingPage::~CaptivePortalBlockingPage() {
  // Need to explicity deny the certificate via the callback, otherwise memory
  // is leaked.
  if (!callback_.is_null()) {
    callback_.Run(false);
    callback_.Reset();
  }
}

const void* CaptivePortalBlockingPage::GetTypeForTesting() const {
  return CaptivePortalBlockingPage::kTypeForTesting;
}

bool CaptivePortalBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

void CaptivePortalBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetString("iconClass", "icon-offline");
  load_time_data->SetString("type", "CAPTIVE_PORTAL");
  load_time_data->SetBoolean("overridable", false);

  // |IsWifiConnection| isn't accurate on some platforms, so always try to get
  // the Wi-Fi SSID even if |IsWifiConnection| is false.
  std::string wifi_ssid = delegate_.get()->GetWiFiSSID();
  bool is_wifi_connection = !wifi_ssid.empty() ||
                            delegate_.get()->IsWifiConnection();

  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_BUTTON_OPEN_LOGIN_PAGE));
  load_time_data->SetString(
      "tabTitle", l10n_util::GetStringUTF16(
                      is_wifi_connection ? IDS_CAPTIVE_PORTAL_HEADING_WIFI
                                         : IDS_CAPTIVE_PORTAL_HEADING_WIRED));
  load_time_data->SetString(
      "heading", l10n_util::GetStringUTF16(
                     is_wifi_connection ? IDS_CAPTIVE_PORTAL_HEADING_WIFI
                                        : IDS_CAPTIVE_PORTAL_HEADING_WIRED));

  if (login_url_.spec() == captive_portal::CaptivePortalDetector::kDefaultURL) {
    // Captive portal may intercept requests without HTTP redirects, in which
    // case the login url would be the same as the captive portal detection url.
    // Don't show the login url in that case.
    if (wifi_ssid.empty()) {
      load_time_data->SetString(
          "primaryParagraph",
          l10n_util::GetStringUTF16(
              is_wifi_connection
                  ? IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIFI
                  : IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIRED));
    } else {
      load_time_data->SetString(
          "primaryParagraph",
          l10n_util::GetStringFUTF16(
              IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIFI_SSID,
              net::EscapeForHTML(base::UTF8ToUTF16(wifi_ssid))));
    }
  } else {
    // Portal redirection was done with HTTP redirects, so show the login URL.
    // If |languages| is empty, punycode in |login_host| will always be decoded.
    std::string languages;
    Profile* profile = Profile::FromBrowserContext(
        web_contents()->GetBrowserContext());
    if (profile)
      languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
    base::string16 login_host = net::IDNToUnicode(login_url_.host(), languages);
    if (base::i18n::IsRTL())
      base::i18n::WrapStringWithLTRFormatting(&login_host);

    if (wifi_ssid.empty()) {
      load_time_data->SetString(
          "primaryParagraph",
          l10n_util::GetStringFUTF16(
              is_wifi_connection ? IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIFI
                                 : IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIRED,
              login_host));
    } else {
      load_time_data->SetString(
          "primaryParagraph",
          l10n_util::GetStringFUTF16(
              IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIFI_SSID,
              net::EscapeForHTML(base::UTF8ToUTF16(wifi_ssid)), login_host));
    }
  }

  // Fill the empty strings to avoid getting debug warnings.
  load_time_data->SetString("openDetails", base::string16());
  load_time_data->SetString("closeDetails", base::string16());
  load_time_data->SetString("explanationParagraph", base::string16());
  load_time_data->SetString("finalParagraph", base::string16());
}

void CaptivePortalBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval) << command;

  if (cmd == CMD_OPEN_LOGIN) {
    RecordUMA(OPEN_LOGIN_PAGE);
    CaptivePortalTabHelper::OpenLoginTabForWebContents(web_contents(), true);
  }
}
