// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_interstitial.h"

#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

using content::BrowserThread;

// static
void SupervisedUserInterstitial::Show(content::WebContents* web_contents,
                                   const GURL& url,
                                   const base::Callback<void(bool)>& callback) {
  SupervisedUserInterstitial* interstitial =
      new SupervisedUserInterstitial(web_contents, url, callback);

  // If Init() does not complete fully, immediately delete the interstitial.
  if (!interstitial->Init())
    delete interstitial;
  // Otherwise |interstitial_page_| is responsible for deleting it.
}

SupervisedUserInterstitial::SupervisedUserInterstitial(
    content::WebContents* web_contents,
    const GURL& url,
    const base::Callback<void(bool)>& callback)
    : web_contents_(web_contents),
      interstitial_page_(NULL),
      url_(url),
      callback_(callback) {}

SupervisedUserInterstitial::~SupervisedUserInterstitial() {}

bool SupervisedUserInterstitial::Init() {
  if (ShouldProceed()) {
    // It can happen that the site was only allowed very recently and the URL
    // filter on the IO thread had not been updated yet. Proceed with the
    // request without showing the interstitial.
    DispatchContinueRequest(true);
    return false;
  }

  InfoBarService* service = InfoBarService::FromWebContents(web_contents_);
  if (service) {
    // Remove all the infobars which are attached to |web_contents_| and for
    // which ShouldExpire() returns true.
    content::LoadCommittedDetails details;
    // |details.is_in_page| is default false, and |details.is_main_frame| is
    // default true. This results in is_navigation_to_different_page() returning
    // true.
    DCHECK(details.is_navigation_to_different_page());
    const content::NavigationController& controller =
        web_contents_->GetController();
    details.entry = controller.GetActiveEntry();
    if (controller.GetLastCommittedEntry()) {
      details.previous_entry_index = controller.GetLastCommittedEntryIndex();
      details.previous_url = controller.GetLastCommittedEntry()->GetURL();
    }
    details.type = content::NAVIGATION_TYPE_NEW_PAGE;
    for (int i = service->infobar_count() - 1; i >= 0; --i) {
      infobars::InfoBar* infobar = service->infobar_at(i);
      if (infobar->delegate()->ShouldExpire(
              InfoBarService::NavigationDetailsFromLoadCommittedDetails(
                  details)))
        service->RemoveInfoBar(infobar);
    }
  }

  // TODO(bauerb): Extract an observer callback on SupervisedUserService for
  // this.
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      base::Bind(&SupervisedUserInterstitial::OnFilteringPrefsChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSupervisedUserManualHosts,
      base::Bind(&SupervisedUserInterstitial::OnFilteringPrefsChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSupervisedUserManualURLs,
      base::Bind(&SupervisedUserInterstitial::OnFilteringPrefsChanged,
                 base::Unretained(this)));

  languages_ = prefs->GetString(prefs::kAcceptLanguages);
  interstitial_page_ =
      content::InterstitialPage::Create(web_contents_, true, url_, this);
  interstitial_page_->Show();

  return true;
}

std::string SupervisedUserInterstitial::GetHTMLContents() {
  base::DictionaryValue strings;
  strings.SetString("blockPageTitle",
                    l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_TITLE));

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);

  bool allow_access_requests = supervised_user_service->AccessRequestsEnabled();
  strings.SetBoolean("allowAccessRequests", allow_access_requests);

  base::string16 custodian =
      base::UTF8ToUTF16(supervised_user_service->GetCustodianName());
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
      IDR_SUPERVISED_USER_BLOCK_INTERSTITIAL_HTML));

  webui::UseVersion2 version;
  return webui::GetI18nTemplateHtml(html, &strings);
}

void SupervisedUserInterstitial::CommandReceived(const std::string& command) {
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
    SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    supervised_user_service->AddAccessRequest(url_);
    DVLOG(1) << "Sent access request for " << url_.spec();

    return;
  }

  NOTREACHED();
}

void SupervisedUserInterstitial::OnProceed() {
  // CHECK instead of DCHECK as defense in depth in case we'd accidentally
  // proceed on a blocked page.
  CHECK(ShouldProceed());
  DispatchContinueRequest(true);
}

void SupervisedUserInterstitial::OnDontProceed() {
  DispatchContinueRequest(false);
}

bool SupervisedUserInterstitial::ShouldProceed() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilterForUIThread();
  return url_filter->GetFilteringBehaviorForURL(url_) !=
         SupervisedUserURLFilter::BLOCK;
}

void SupervisedUserInterstitial::OnFilteringPrefsChanged() {
  if (ShouldProceed())
    interstitial_page_->Proceed();
}

void SupervisedUserInterstitial::DispatchContinueRequest(
    bool continue_request) {
  // If there is no history entry to go back to, close the tab instead.
  int nav_entry_count = web_contents_->GetController().GetEntryCount();
  if (!continue_request && nav_entry_count == 0)
    web_contents_->Close();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback_, continue_request));
}
