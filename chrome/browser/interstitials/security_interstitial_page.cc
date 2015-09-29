// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/security_interstitial_page.h"

#include "base/i18n/rtl.h"

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace interstitials {
const char kBoxChecked[] = "boxchecked";
const char kDisplayCheckBox[] = "displaycheckbox";
const char kOptInLink[] = "optInLink";
const char kPrivacyLinkHtml[] =
    "<a id=\"privacy-link\" href=\"\" onclick=\"sendCommand(%d); "
    "return false;\" onmousedown=\"return false;\">%s</a>";
}

using content::OpenURLParams;
using content::Referrer;

SecurityInterstitialPage::SecurityInterstitialPage(
    content::WebContents* web_contents,
    const GURL& request_url)
    : web_contents_(web_contents),
      request_url_(request_url),
      interstitial_page_(NULL),
      create_view_(true) {
  // Creating interstitial_page_ without showing it leaks memory, so don't
  // create it here.
}

SecurityInterstitialPage::~SecurityInterstitialPage() {
}

content::InterstitialPage* SecurityInterstitialPage::interstitial_page() const {
  return interstitial_page_;
}

content::WebContents* SecurityInterstitialPage::web_contents() const {
  return web_contents_;
}

GURL SecurityInterstitialPage::request_url() const {
  return request_url_;
}

void SecurityInterstitialPage::DontCreateViewForTesting() {
  create_view_ = false;
}

void SecurityInterstitialPage::Show() {
  DCHECK(!interstitial_page_);
  interstitial_page_ = content::InterstitialPage::Create(
      web_contents_, ShouldCreateNewNavigation(), request_url_, this);
  if (!create_view_)
    interstitial_page_->DontCreateViewForTesting();
  interstitial_page_->Show();
}

void SecurityInterstitialPage::SetReportingPreference(bool report) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PrefService* pref = profile->GetPrefs();
  pref->SetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled, report);
  metrics_helper()->RecordUserInteraction(
      report ? security_interstitials::MetricsHelper::
                   SET_EXTENDED_REPORTING_ENABLED
             : security_interstitials::MetricsHelper::
                   SET_EXTENDED_REPORTING_DISABLED);
}

bool SecurityInterstitialPage::IsPrefEnabled(const char* pref) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return profile->GetPrefs()->GetBoolean(pref);
}

void SecurityInterstitialPage::OpenExtendedReportingPrivacyPolicy() {
  metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::SHOW_PRIVACY_POLICY);
  GURL privacy_url(
      l10n_util::GetStringUTF8(IDS_SAFE_BROWSING_PRIVACY_POLICY_URL));
  privacy_url = google_util::AppendGoogleLocaleParam(
      privacy_url, g_browser_process->GetApplicationLocale());
  OpenURLParams params(privacy_url, Referrer(), CURRENT_TAB,
                       ui::PAGE_TRANSITION_LINK, false);
  web_contents()->OpenURL(params);
}

security_interstitials::MetricsHelper*
SecurityInterstitialPage::metrics_helper() const {
  return metrics_helper_.get();
}

void SecurityInterstitialPage::set_metrics_helper(
    scoped_ptr<security_interstitials::MetricsHelper> metrics_helper) {
  metrics_helper_ = metrics_helper.Pass();
}

base::string16 SecurityInterstitialPage::GetFormattedHostName() const {
  std::string languages;
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (profile)
    languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
  base::string16 host =
      url_formatter::IDNToUnicode(request_url_.host(), languages);
  if (base::i18n::IsRTL())
    base::i18n::WrapStringWithLTRFormatting(&host);
  return host;
}

std::string SecurityInterstitialPage::GetHTMLContents() {
  base::DictionaryValue load_time_data;
  PopulateInterstitialStrings(&load_time_data);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &load_time_data);
  std::string html = ResourceBundle::GetSharedInstance()
                         .GetRawDataResource(IDR_SECURITY_INTERSTITIAL_HTML)
                         .as_string();
  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetI18nTemplateHtml(html, &load_time_data);
}
