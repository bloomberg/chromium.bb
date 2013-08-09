// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_interstitial.h"

#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
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
      weak_ptr_factory_(this),
      callback_(callback) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  languages_ = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);

  interstitial_page_ =
      content::InterstitialPage::Create(web_contents, true, url_, this);
  interstitial_page_->Show();
}

ManagedModeInterstitial::~ManagedModeInterstitial() {}

std::string ManagedModeInterstitial::GetHTMLContents() {
  DictionaryValue strings;
  strings.SetString("blockPageTitle",
                    l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_TITLE));

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);

  bool allow_access_requests = managed_user_service->AccessRequestsEnabled();
  strings.SetBoolean("allowAccessRequests", allow_access_requests);

  string16 custodian = UTF8ToUTF16(managed_user_service->GetCustodianName());
  strings.SetString(
      "blockPageMessage",
      allow_access_requests
          ? l10n_util::GetStringFUTF16(IDS_BLOCK_INTERSTITIAL_MESSAGE,
                                       custodian)
          : l10n_util::GetStringUTF16(
                IDS_BLOCK_INTERSTITIAL_MESSAGE_ACCESS_REQUESTS_DISABLED));

  strings.SetString("backButton", l10n_util::GetStringUTF16(IDS_BACK_BUTTON));
  strings.SetString(
      "requestAccessButton",
      l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_REQUEST_ACCESS_BUTTON));

  strings.SetString(
      "requestSentMessage",
      l10n_util::GetStringFUTF16(IDS_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE,
                                 custodian));

  webui::SetFontAndTextDirection(&strings);

  base::StringPiece html(ResourceBundle::GetSharedInstance().GetRawDataResource(
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
    ACCESS_REQUEST,
    HISTOGRAM_BOUNDING_VALUE
  };

  if (command == "\"back\"") {
    UMA_HISTOGRAM_ENUMERATION("ManagedMode.BlockingInterstitialCommand",
                              BACK,
                              HISTOGRAM_BOUNDING_VALUE);
    interstitial_page_->DontProceed();
    return;
  }

  if (command == "\"request\"") {
    UMA_HISTOGRAM_ENUMERATION("ManagedMode.BlockingInterstitialCommand",
                              ACCESS_REQUEST,
                              HISTOGRAM_BOUNDING_VALUE);

    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    ManagedUserService* managed_user_service =
        ManagedUserServiceFactory::GetForProfile(profile);
    managed_user_service->AddAccessRequest(url_);
    DVLOG(1) << "Sent access request for " << url_.spec();

    return;
  }

  NOTREACHED();
}

void ManagedModeInterstitial::OnProceed() { NOTREACHED(); }

void ManagedModeInterstitial::OnDontProceed() {
  DispatchContinueRequest(false);
}

void ManagedModeInterstitial::DispatchContinueRequest(bool continue_request) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback_, continue_request));
}
