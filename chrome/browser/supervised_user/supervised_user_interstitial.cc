// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_interstitial.h"

#include <stddef.h>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/supervised_user/child_accounts/child_account_feedback_reporter_android.h"
#elif !defined(OS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
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
 public:
  static void MaybeClose(WebContents* web_contents) {
    // Close the tab if there is no history entry to go back to and there is a
    // browser for the tab (which is not the case for example in a <webview>).
    if (!web_contents->GetController().IsInitialBlankNavigation())
      return;

#if !defined(OS_ANDROID)
    if (!chrome::FindBrowserWithWebContents(web_contents))
      return;
#endif
    TabCloser::CreateForWebContents(web_contents);
  }

 private:
  friend class content::WebContentsUserData<TabCloser>;

  explicit TabCloser(WebContents* web_contents)
      : web_contents_(web_contents), weak_ptr_factory_(this) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&TabCloser::CloseTabImpl, weak_ptr_factory_.GetWeakPtr()));
  }
  ~TabCloser() override {}

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

content::InterstitialPageDelegate::TypeID
    SupervisedUserInterstitial::kTypeForTesting =
        &SupervisedUserInterstitial::kTypeForTesting;

// static
void SupervisedUserInterstitial::Show(
    WebContents* web_contents,
    const GURL& url,
    SupervisedUserURLFilter::FilteringBehaviorReason reason,
    const base::Callback<void(bool)>& callback) {
  SupervisedUserInterstitial* interstitial =
      new SupervisedUserInterstitial(web_contents, url, reason, callback);

  // If Init() does not complete fully, immediately delete the interstitial.
  if (!interstitial->Init())
    delete interstitial;
  // Otherwise |interstitial_page_| is responsible for deleting it.
}

SupervisedUserInterstitial::SupervisedUserInterstitial(
    WebContents* web_contents,
    const GURL& url,
    SupervisedUserURLFilter::FilteringBehaviorReason reason,
    const base::Callback<void(bool)>& callback)
    : web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      interstitial_page_(NULL),
      url_(url),
      reason_(reason),
      callback_(callback),
      weak_ptr_factory_(this) {}

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

// static
std::string SupervisedUserInterstitial::GetHTMLContents(
    Profile* profile,
    SupervisedUserURLFilter::FilteringBehaviorReason reason) {
  base::DictionaryValue strings;
  strings.SetString("blockPageTitle",
                    l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_TITLE));

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);

  bool allow_access_requests = supervised_user_service->AccessRequestsEnabled();
  strings.SetBoolean("allowAccessRequests", allow_access_requests);

  std::string profile_image_url = profile->GetPrefs()->GetString(
      prefs::kSupervisedUserCustodianProfileImageURL);
  strings.SetString("avatarURL1x", BuildAvatarImageUrl(profile_image_url,
                                                       kAvatarSize1x));
  strings.SetString("avatarURL2x", BuildAvatarImageUrl(profile_image_url,
                                                       kAvatarSize2x));

  std::string profile_image_url2 = profile->GetPrefs()->GetString(
      prefs::kSupervisedUserSecondCustodianProfileImageURL);
  strings.SetString("secondAvatarURL1x", BuildAvatarImageUrl(profile_image_url2,
                                                             kAvatarSize1x));
  strings.SetString("secondAvatarURL2x", BuildAvatarImageUrl(profile_image_url2,
                                                             kAvatarSize2x));

  bool is_child_account = profile->IsChild();

  base::string16 custodian =
      base::UTF8ToUTF16(supervised_user_service->GetCustodianName());
  base::string16 second_custodian =
      base::UTF8ToUTF16(supervised_user_service->GetSecondCustodianName());
  base::string16 custodian_email =
      base::UTF8ToUTF16(supervised_user_service->GetCustodianEmailAddress());
  base::string16 second_custodian_email = base::UTF8ToUTF16(
      supervised_user_service->GetSecondCustodianEmailAddress());
  strings.SetString("custodianName", custodian);
  strings.SetString("custodianEmail", custodian_email);
  strings.SetString("secondCustodianName", second_custodian);
  strings.SetString("secondCustodianEmail", second_custodian_email);

  base::string16 block_message;
  if (allow_access_requests) {
    if (is_child_account) {
      block_message = l10n_util::GetStringUTF16(
          second_custodian.empty()
              ? IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_SINGLE_PARENT
              : IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_MULTI_PARENT);
    } else {
      block_message = l10n_util::GetStringFUTF16(
          IDS_BLOCK_INTERSTITIAL_MESSAGE, custodian);
    }
  } else {
    block_message = l10n_util::GetStringUTF16(
        IDS_BLOCK_INTERSTITIAL_MESSAGE_ACCESS_REQUESTS_DISABLED);
  }
  strings.SetString("blockPageMessage", block_message);
  strings.SetString("blockReasonMessage", l10n_util::GetStringUTF16(
      SupervisedUserURLFilter::GetBlockMessageID(
          reason, is_child_account, second_custodian.empty())));
  strings.SetString("blockReasonHeader", l10n_util::GetStringUTF16(
      SupervisedUserURLFilter::GetBlockHeaderID(reason)));
  bool show_feedback = false;
#if defined(GOOGLE_CHROME_BUILD)
  show_feedback =
      is_child_account && SupervisedUserURLFilter::ReasonIsAutomatic(reason);
#endif
  strings.SetBoolean("showFeedbackLink", show_feedback);
  strings.SetString("feedbackLink",
      l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_SEND_FEEDBACK));

  strings.SetString("backButton", l10n_util::GetStringUTF16(IDS_BACK_BUTTON));
  strings.SetString("requestAccessButton", l10n_util::GetStringUTF16(
      IDS_BLOCK_INTERSTITIAL_REQUEST_ACCESS_BUTTON));

  strings.SetString("showDetailsLink", l10n_util::GetStringUTF16(
      IDS_BLOCK_INTERSTITIAL_SHOW_DETAILS));
  strings.SetString("hideDetailsLink", l10n_util::GetStringUTF16(
        IDS_BLOCK_INTERSTITIAL_HIDE_DETAILS));

  base::string16 request_sent_message;
  base::string16 request_failed_message;
  if (is_child_account) {
    if (second_custodian.empty()) {
      request_sent_message = l10n_util::GetStringUTF16(
          IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_SINGLE_PARENT);
      request_failed_message = l10n_util::GetStringUTF16(
          IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT);
    } else {
      request_sent_message = l10n_util::GetStringUTF16(
          IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_MULTI_PARENT);
      request_failed_message = l10n_util::GetStringUTF16(
          IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT);
    }
  } else {
    request_sent_message = l10n_util::GetStringFUTF16(
        IDS_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE, custodian);
    request_failed_message = l10n_util::GetStringFUTF16(
        IDS_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE, custodian);
  }
  strings.SetString("requestSentMessage", request_sent_message);
  strings.SetString("requestFailedMessage", request_failed_message);

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &strings);

  std::string html =
      ResourceBundle::GetSharedInstance()
          .GetRawDataResource(IDR_SUPERVISED_USER_BLOCK_INTERSTITIAL_HTML)
          .as_string();
  webui::AppendWebUiCssTextDefaults(&html);

  return webui::GetI18nTemplateHtml(html, &strings);
}

std::string SupervisedUserInterstitial::GetHTMLContents() {
  return GetHTMLContents(profile_, reason_);
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

    // The interstitial's reference to the WebContents will go away after the
    // DontProceed call.
    WebContents* web_contents = web_contents_;
    DCHECK(web_contents->GetController().GetTransientEntry());
    interstitial_page_->DontProceed();

    TabCloser::MaybeClose(web_contents);
    return;
  }

  if (command == "\"request\"") {
    UMA_HISTOGRAM_ENUMERATION("ManagedMode.BlockingInterstitialCommand",
                              ACCESS_REQUEST,
                              HISTOGRAM_BOUNDING_VALUE);

    SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    supervised_user_service->AddURLAccessRequest(
        url_, base::Bind(&SupervisedUserInterstitial::OnAccessRequestAdded,
                         weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  base::string16 second_custodian =
      base::UTF8ToUTF16(supervised_user_service->GetSecondCustodianName());

  if (command == "\"feedback\"") {
    base::string16 reason = l10n_util::GetStringUTF16(
        SupervisedUserURLFilter::GetBlockMessageID(
            reason_, true, second_custodian.empty()));
    std::string message = l10n_util::GetStringFUTF8(
        IDS_BLOCK_INTERSTITIAL_DEFAULT_FEEDBACK_TEXT, reason);
#if BUILDFLAG(ANDROID_JAVA_UI)
    ReportChildAccountFeedback(web_contents_, message, url_);
#else
    chrome::ShowFeedbackPage(chrome::FindBrowserWithWebContents(web_contents_),
                           message, std::string());
#endif
    return;
  }

  NOTREACHED();
}

void SupervisedUserInterstitial::OnProceed() {
  DispatchContinueRequest(true);
}

void SupervisedUserInterstitial::OnDontProceed() {
  DispatchContinueRequest(false);
}

content::InterstitialPageDelegate::TypeID
SupervisedUserInterstitial::GetTypeForTesting() const {
  return SupervisedUserInterstitial::kTypeForTesting;
}

void SupervisedUserInterstitial::OnURLFilterChanged() {
  if (ShouldProceed())
    interstitial_page_->Proceed();
}

void SupervisedUserInterstitial::OnAccessRequestAdded(bool success) {
  VLOG(1) << "Sent access request for " << url_.spec()
          << (success ? " successfully" : " unsuccessfully");
  std::string jsFunc =
      base::StringPrintf("setRequestStatus(%s);", success ? "true" : "false");
  interstitial_page_->GetMainFrame()->ExecuteJavaScript(
      base::ASCIIToUTF16(jsFunc));
}

bool SupervisedUserInterstitial::ShouldProceed() {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilterForUIThread();
  SupervisedUserURLFilter::FilteringBehavior behavior;
  if (url_filter->HasAsyncURLChecker()) {
    if (!url_filter->GetManualFilteringBehaviorForURL(url_, &behavior))
      return false;
  } else {
    behavior = url_filter->GetFilteringBehaviorForURL(url_);
  }
  return behavior != SupervisedUserURLFilter::BLOCK;
}

void SupervisedUserInterstitial::DispatchContinueRequest(
    bool continue_request) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  supervised_user_service->RemoveObserver(this);

  if (!callback_.is_null())
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback_, continue_request));

  // After this, the WebContents may be destroyed. Make sure we don't try to use
  // it again.
  web_contents_ = NULL;
}
