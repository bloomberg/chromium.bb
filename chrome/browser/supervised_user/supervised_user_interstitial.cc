// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_interstitial.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using content::BrowserThread;
using content::WebContents;

namespace {

static const int kAvatarSize1x = 45;
static const int kAvatarSize2x = 90;

std::string BuildAvatarImageUrl(const std::string& url, int size) {
  std::string result = url;
  size_t slash = result.rfind('/');
  if (slash != std::string::npos)
    result.insert(slash, "/s" + base::IntToString(size));
  return result;
}

class TabCloser : public content::WebContentsUserData<TabCloser> {
  // To use, call TabCloser::CreateForWebContents.
 private:
  friend class content::WebContentsUserData<TabCloser>;

  explicit TabCloser(WebContents* web_contents)
      : web_contents_(web_contents), weak_ptr_factory_(this) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&TabCloser::CloseTabImpl, weak_ptr_factory_.GetWeakPtr()));
  }
  virtual ~TabCloser() {}

  void CloseTabImpl() {
    // On Android, FindBrowserWithWebContents and TabStripModel don't exist.
#if !defined(OS_ANDROID)
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
    DCHECK(browser);
    TabStripModel* tab_strip = browser->tab_strip_model();
    DCHECK_NE(TabStripModel::kNoTab,
              tab_strip->GetIndexOfWebContents(web_contents_));
    if (tab_strip->count() <= 1) {
      // Don't close the last tab in the window.
      web_contents_->RemoveUserData(UserDataKey());
      return;
    }
#endif
    web_contents_->Close();
  }

  WebContents* web_contents_;
  base::WeakPtrFactory<TabCloser> weak_ptr_factory_;
};

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TabCloser);

// static
void SupervisedUserInterstitial::Show(
    WebContents* web_contents,
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
    WebContents* web_contents,
    const GURL& url,
    const base::Callback<void(bool)>& callback)
    : web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      interstitial_page_(NULL),
      url_(url),
      callback_(callback) {}

SupervisedUserInterstitial::~SupervisedUserInterstitial() {
  DCHECK(!web_contents_);
}

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

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  supervised_user_service->AddObserver(this);

  interstitial_page_ =
      content::InterstitialPage::Create(web_contents_, true, url_, this);
  interstitial_page_->Show();

  return true;
}

std::string SupervisedUserInterstitial::GetHTMLContents() {
  base::DictionaryValue strings;
  strings.SetString("blockPageTitle",
                    l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_TITLE));

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);

  bool allow_access_requests = supervised_user_service->AccessRequestsEnabled();
  strings.SetBoolean("allowAccessRequests", allow_access_requests);

  std::string profile_image_url = profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserCustodianProfileImageURL);
  strings.SetString("avatarURL1x", BuildAvatarImageUrl(profile_image_url,
                                                       kAvatarSize1x));
  strings.SetString("avatarURL2x", BuildAvatarImageUrl(profile_image_url,
                                                       kAvatarSize2x));

  std::string profile_image_url2 = profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserSecondCustodianProfileImageURL);
  strings.SetString("secondAvatarURL1x", BuildAvatarImageUrl(profile_image_url2,
                                                             kAvatarSize1x));
  strings.SetString("secondAvatarURL2x", BuildAvatarImageUrl(profile_image_url2,
                                                             kAvatarSize2x));

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

    // Close the tab if there is no history entry to go back to.
    DCHECK(web_contents_->GetController().GetTransientEntry());
    if (web_contents_->GetController().GetEntryCount() == 1)
      TabCloser::CreateForWebContents(web_contents_);

    interstitial_page_->DontProceed();
    return;
  }

  if (command == "\"request\"") {
    UMA_HISTOGRAM_ENUMERATION("ManagedMode.BlockingInterstitialCommand",
                              ACCESS_REQUEST,
                              HISTOGRAM_BOUNDING_VALUE);

    SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
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

void SupervisedUserInterstitial::OnURLFilterChanged() {
  if (ShouldProceed())
    interstitial_page_->Proceed();
}

bool SupervisedUserInterstitial::ShouldProceed() {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilterForUIThread();
  return url_filter->GetFilteringBehaviorForURL(url_) !=
         SupervisedUserURLFilter::BLOCK;
}

void SupervisedUserInterstitial::DispatchContinueRequest(
    bool continue_request) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  supervised_user_service->RemoveObserver(this);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback_, continue_request));

  // After this, the WebContents may be destroyed. Make sure we don't try to use
  // it again.
  web_contents_ = NULL;
}
