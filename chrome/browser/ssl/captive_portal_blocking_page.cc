// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/captive_portal_blocking_page.h"

#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
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

const char kOpenLoginPageCommand[] = "openLoginPage";

} // namespace

// static
const void* CaptivePortalBlockingPage::kTypeForTesting =
    &CaptivePortalBlockingPage::kTypeForTesting;

CaptivePortalBlockingPage::CaptivePortalBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    const base::Callback<void(bool)>& callback)
    : SecurityInterstitialPage(web_contents, request_url),
      callback_(callback) {
#if !defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  NOTREACHED();
#endif
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

  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_BUTTON_OPEN_LOGIN_PAGE));
  load_time_data->SetString("tabTitle",
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_TITLE));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH));
  load_time_data->SetString("heading",
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_HEADING));

  // Fill the empty strings to avoid getting debug warnings.
  load_time_data->SetString("openDetails", base::string16());
  load_time_data->SetString("closeDetails", base::string16());
  load_time_data->SetString("explanationParagraph", base::string16());
  load_time_data->SetString("finalParagraph", base::string16());
}

void CaptivePortalBlockingPage::CommandReceived(const std::string& command) {
  // The response has quotes around it.
  if (command == std::string("\"") + kOpenLoginPageCommand + "\"") {
    RecordUMA(OPEN_LOGIN_PAGE);
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
    CaptivePortalTabHelper::OpenLoginTabForWebContents(web_contents(), true);
#endif
  }
}
