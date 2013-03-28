// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_interstitial.h"

#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/jstemplate_builder.h"
#include "ui/webui/web_ui_util.h"

using content::BrowserThread;

ManagedModeInterstitial::ManagedModeInterstitial(
    content::WebContents* web_contents,
    const GURL& url,
    const base::Callback<void(bool)>& callback)
    : web_contents_(web_contents),
      url_(url),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      callback_(callback) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  languages_ = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);

  interstitial_page_ = content::InterstitialPage::Create(
      web_contents, true, url_, this);
  interstitial_page_->Show();
}

ManagedModeInterstitial::~ManagedModeInterstitial() {}

void ManagedModeInterstitial::GoToNewTabPage() {
  web_contents_->GetDelegate()->OpenURLFromTab(
      web_contents_,
      content::OpenURLParams(GURL(chrome::kChromeUINewTabURL),
                             content::Referrer(),
                             CURRENT_TAB,
                             content::PAGE_TRANSITION_LINK,
                             false));
}

std::string ManagedModeInterstitial::GetHTMLContents() {
  DictionaryValue strings;
  strings.SetString("blockPageTitle",
                    l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_TITLE));
  strings.SetString("blockPageMessage",
                    l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_MESSAGE));
  strings.SetString("blockedUrl", net::FormatUrl(url_, languages_));
  strings.SetString("bypassBlockMessage",
                    l10n_util::GetStringUTF16(IDS_BYPASS_BLOCK_MESSAGE));
  strings.SetString("bypassBlockButton",
                    l10n_util::GetStringUTF16(IDS_BYPASS_BLOCK_BUTTON));
  strings.SetString("backButton", l10n_util::GetStringUTF16(IDS_BACK_BUTTON));
  strings.SetString(
      "contentPacksSectionButton",
      l10n_util::GetStringUTF16(IDS_CONTENT_PACKS_SECTION_BUTTON));
  webui::SetFontAndTextDirection(&strings);

  base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_MANAGED_MODE_BLOCK_INTERSTITIAL_HTML));

  webui::UseVersion2 version;
  return webui::GetI18nTemplateHtml(html, &strings);
}

void ManagedModeInterstitial::CommandReceived(const std::string& command) {
  // For use in histograms.
  enum Commands {
    PREVIEW,
    BACK,
    NTP,
    HISTOGRAM_BOUNDING_VALUE
  };

  if (command == "\"preview\"") {
    UMA_HISTOGRAM_ENUMERATION("ManagedMode.BlockingInterstitialCommand",
                              PREVIEW,
                              HISTOGRAM_BOUNDING_VALUE);
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    ManagedUserService* service =
        ManagedUserServiceFactory::GetForProfile(profile);
    service->RequestAuthorization(
        web_contents_,
        base::Bind(&ManagedModeInterstitial::OnAuthorizationResult,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (command == "\"back\"") {
    UMA_HISTOGRAM_ENUMERATION("ManagedMode.BlockingInterstitialCommand",
                              BACK,
                              HISTOGRAM_BOUNDING_VALUE);
    interstitial_page_->DontProceed();
    return;
  }

  if (command == "\"ntp\"") {
    UMA_HISTOGRAM_ENUMERATION("ManagedMode.BlockingInterstitialCommand",
                              NTP,
                              HISTOGRAM_BOUNDING_VALUE);
    GoToNewTabPage();
    return;
  }

  NOTREACHED();
}

void ManagedModeInterstitial::OnProceed() {
  callback_.Run(true);
}

void ManagedModeInterstitial::OnDontProceed() {
  callback_.Run(false);
}

void ManagedModeInterstitial::OnAuthorizationResult(bool success) {
  if (success)
    interstitial_page_->Proceed();
}
