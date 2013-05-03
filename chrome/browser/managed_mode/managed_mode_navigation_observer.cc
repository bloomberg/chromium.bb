// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/infobars/simple_alert_infobar_delegate.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/managed_mode/managed_mode_interstitial.h"
#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/frame_navigate_params.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::UserMetricsAction;

namespace {


// Helpers --------------------------------------------------------------------

void GoBackToSafety(content::WebContents* web_contents) {
  // For now, just go back one page (the user didn't retreat from that page,
  // so it should be okay).
  content::NavigationController* controller =
      &web_contents->GetController();
  if (controller->CanGoBack()) {
    controller->GoBack();
    return;
  }

  // If we can't go back (because we opened a new tab), try to close the tab.
  // If this is the last tab on this desktop, open a new window.
  chrome::HostDesktopType host_desktop_type =
      chrome::GetHostDesktopTypeForNativeView(
          web_contents->GetView()->GetNativeView());
  const BrowserList* browser_list = BrowserList::GetInstance(host_desktop_type);
  if (browser_list->size() == 1) {
    Browser* browser = browser_list->get(0);
    DCHECK(browser == chrome::FindBrowserWithWebContents(web_contents));
    if (browser->tab_strip_model()->count() == 1)
      chrome::NewEmptyWindow(browser->profile(), browser->host_desktop_type());
  }

  web_contents->GetDelegate()->CloseContents(web_contents);
}


// ManagedModeWarningInfobarDelegate ------------------------------------------

class ManagedModeWarningInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a managed mode warning delegate and adds it to |infobar_service|.
  // Returns the delegate if it was successfully added.
  static InfoBarDelegate* Create(InfoBarService* infobar_service,
                                 int last_allowed_page);

 private:
  explicit ManagedModeWarningInfobarDelegate(InfoBarService* infobar_service,
                                             int last_allowed_page);
  virtual ~ManagedModeWarningInfobarDelegate();

  // ConfirmInfoBarDelegate overrides:
  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  int last_allowed_page_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeWarningInfobarDelegate);
};

// static
InfoBarDelegate* ManagedModeWarningInfobarDelegate::Create(
    InfoBarService* infobar_service, int last_allowed_page) {
  scoped_ptr<InfoBarDelegate> delegate(
    new ManagedModeWarningInfobarDelegate(infobar_service, last_allowed_page));
  return infobar_service->AddInfoBar(delegate.Pass());
}

ManagedModeWarningInfobarDelegate::ManagedModeWarningInfobarDelegate(
    InfoBarService* infobar_service,
    int last_allowed_page)
    : ConfirmInfoBarDelegate(infobar_service),
      last_allowed_page_(last_allowed_page) {}

ManagedModeWarningInfobarDelegate::~ManagedModeWarningInfobarDelegate() {}

bool ManagedModeWarningInfobarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  // ManagedModeNavigationObserver removes us below.
  return false;
}

void ManagedModeWarningInfobarDelegate::InfoBarDismissed() {
  ManagedModeNavigationObserver* observer =
      ManagedModeNavigationObserver::FromWebContents(web_contents());
  observer->WarnInfobarDismissed();
}

string16 ManagedModeWarningInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MANAGED_MODE_WARNING_MESSAGE);
}

int ManagedModeWarningInfobarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 ManagedModeWarningInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_MANAGED_MODE_GO_BACK_ACTION);
}

bool ManagedModeWarningInfobarDelegate::Accept() {
  GoBackToSafety(web_contents());

  return false;
}

bool ManagedModeWarningInfobarDelegate::Cancel() {
  NOTREACHED();
  return false;
}


// ManagedModePreviewInfobarDelegate ------------------------------------------

class ManagedModePreviewInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a managed mode preview delegate and adds it to |infobar_service|.
  // Returns the delegate if it was successfully added.
  static InfoBarDelegate* Create(InfoBarService* infobar_service);

 private:
  // For use in histograms.
  enum PreviewInfobarCommand {
    INFOBAR_ACCEPT,
    INFOBAR_CANCEL,
    INFOBAR_HISTOGRAM_BOUNDING_VALUE
  };

  explicit ManagedModePreviewInfobarDelegate(InfoBarService* infobar_service);
  virtual ~ManagedModePreviewInfobarDelegate();

  // ConfirmInfoBarDelegate overrides:
  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ManagedModePreviewInfobarDelegate);
};

// static
InfoBarDelegate* ManagedModePreviewInfobarDelegate::Create(
    InfoBarService* infobar_service) {
  scoped_ptr<InfoBarDelegate> delegate(
      new ManagedModePreviewInfobarDelegate(infobar_service));
  return infobar_service->AddInfoBar(delegate.Pass());
}

ManagedModePreviewInfobarDelegate::ManagedModePreviewInfobarDelegate(
    InfoBarService* infobar_service)
    : ConfirmInfoBarDelegate(infobar_service) {}

ManagedModePreviewInfobarDelegate::~ManagedModePreviewInfobarDelegate() {}

bool ManagedModePreviewInfobarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  // ManagedModeNavigationObserver removes us below.
  return false;
}

void ManagedModePreviewInfobarDelegate::InfoBarDismissed() {
  ManagedModeNavigationObserver* observer =
      ManagedModeNavigationObserver::FromWebContents(web_contents());
  observer->PreviewInfobarDismissed();
}

string16 ManagedModePreviewInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MANAGED_MODE_PREVIEW_MESSAGE);
}

int ManagedModePreviewInfobarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 ManagedModePreviewInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      (button == BUTTON_OK) ? IDS_MANAGED_MODE_PREVIEW_ACCEPT
                            : IDS_MANAGED_MODE_GO_BACK_ACTION);
}

bool ManagedModePreviewInfobarDelegate::Accept() {
  ManagedModeNavigationObserver* observer =
      ManagedModeNavigationObserver::FromWebContents(web_contents());
  UMA_HISTOGRAM_ENUMERATION("ManagedMode.PreviewInfobarCommand",
                            INFOBAR_ACCEPT,
                            INFOBAR_HISTOGRAM_BOUNDING_VALUE);
  observer->AddSavedURLsToWhitelistAndClearState();
  return true;
}

bool ManagedModePreviewInfobarDelegate::Cancel() {
  ManagedModeNavigationObserver* observer =
      ManagedModeNavigationObserver::FromWebContents(web_contents());
  UMA_HISTOGRAM_ENUMERATION("ManagedMode.PreviewInfobarCommand",
                            INFOBAR_CANCEL,
                            INFOBAR_HISTOGRAM_BOUNDING_VALUE);
  GoBackToSafety(web_contents());
  observer->ClearObserverState();
  return false;
}

}  // namespace


// ManagedModeNavigationObserver ----------------------------------------------

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagedModeNavigationObserver);

ManagedModeNavigationObserver::~ManagedModeNavigationObserver() {
  RemoveTemporaryException();
}

ManagedModeNavigationObserver::ManagedModeNavigationObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      warn_infobar_delegate_(NULL),
      preview_infobar_delegate_(NULL),
      state_(RECORDING_URLS_BEFORE_PREVIEW),
      is_elevated_(false),
      last_allowed_page_(-1),
      finished_redirects_(false) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  managed_user_service_ = ManagedUserServiceFactory::GetForProfile(profile);
  if (!managed_user_service_->ProfileIsManaged())
    is_elevated_ = true;
  url_filter_ = managed_user_service_->GetURLFilterForUIThread();
}

void ManagedModeNavigationObserver::AddTemporaryException() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(web_contents());

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeResourceThrottle::AddTemporaryException,
                 web_contents()->GetRenderProcessHost()->GetID(),
                 web_contents()->GetRenderViewHost()->GetRoutingID(),
                 last_url_));
}

void ManagedModeNavigationObserver::RemoveTemporaryException() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // When closing the browser web_contents() may return NULL so guard against
  // that.
  if (!web_contents())
    return;

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeResourceThrottle::RemoveTemporaryException,
                 web_contents()->GetRenderProcessHost()->GetID(),
                 web_contents()->GetRenderViewHost()->GetRoutingID()));
}

void ManagedModeNavigationObserver::WarnInfobarDismissed() {
  DCHECK(warn_infobar_delegate_);
  warn_infobar_delegate_ = NULL;
}

void ManagedModeNavigationObserver::PreviewInfobarDismissed() {
  DCHECK(preview_infobar_delegate_);
  preview_infobar_delegate_ = NULL;
}

void ManagedModeNavigationObserver::AddSavedURLsToWhitelistAndClearState() {
  std::vector<GURL> urls;
  for (std::set<GURL>::const_iterator it = navigated_urls_.begin();
       it != navigated_urls_.end();
       ++it) {
    if (it->host() != last_url_.host())
      urls.push_back(*it);
  }
  managed_user_service_->SetManualBehaviorForURLs(
      urls, ManagedUserService::MANUAL_ALLOW);
  if (last_url_.is_valid()) {
    std::vector<std::string> hosts;
    hosts.push_back(last_url_.host());
    managed_user_service_->SetManualBehaviorForHosts(
        hosts, ManagedUserService::MANUAL_ALLOW);
  }
  ClearObserverState();
}

bool ManagedModeNavigationObserver::is_elevated() const {
#if defined(OS_CHROMEOS)
  return false;
#else
  return is_elevated_;
#endif
}

void ManagedModeNavigationObserver::set_elevated(bool is_elevated) {
#if defined(OS_CHROMEOS)
  NOTREACHED();
#else
  is_elevated_ = is_elevated;
#endif
}

void ManagedModeNavigationObserver::AddURLToPatternList(const GURL& url) {
  navigated_urls_.insert(url);
  last_url_ = url;
}

void ManagedModeNavigationObserver::SetStateToRecordingAfterPreview() {
  state_ = RECORDING_URLS_AFTER_PREVIEW;
}

bool ManagedModeNavigationObserver::CanTemporarilyNavigateHost(
    const GURL& url) {
  return last_url_.host() == url.host();
}

bool ManagedModeNavigationObserver::ShouldStayElevatedForURL(
    const GURL& navigation_url) {
  std::string url = navigation_url.spec();
  // Handle chrome:// URLs specially.
  if (navigation_url.host() == "chrome") {
    // The path contains the actual host name, but starts with a "/". Remove
    // the "/".
    url = navigation_url.path().substr(1);
  }

  // Check if any of the special URLs is a prefix of |url|.
  return StartsWithASCII(url, chrome::kChromeUIHistoryHost, false) ||
         StartsWithASCII(url, chrome::kChromeUIExtensionsHost, false) ||
         StartsWithASCII(url, chrome::kChromeUISettingsHost, false) ||
         StartsWithASCII(url, extension_urls::kGalleryBrowsePrefix, false);
}

void ManagedModeNavigationObserver::ClearObserverState() {
  if (preview_infobar_delegate_) {
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents());
    infobar_service->RemoveInfoBar(preview_infobar_delegate_);
    preview_infobar_delegate_ = NULL;
  }
  navigated_urls_.clear();
  last_url_ = GURL();
  state_ = RECORDING_URLS_BEFORE_PREVIEW;
  // TODO(sergiu): Remove these logging calls once this is stable.
  DVLOG(1) << "Clearing observer state";
  RemoveTemporaryException();
}

void ManagedModeNavigationObserver::NavigateToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  DVLOG(1) << "NavigateToPendingEntry " << url.spec();
  // This method gets called only when the user goes back and forward and when
  // the user types a new URL. So do the main work in
  // ProvisionalChangeToMainFrameUrl and only check that the user didn't go
  // back to a blocked site here.
  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(url);
  int current_index = web_contents()->GetController().GetCurrentEntryIndex();
  // First check that the user didn't go back to a blocked page, then check
  // that the navigation is allowed.
  if (current_index <= last_allowed_page_ ||
      (behavior == ManagedModeURLFilter::BLOCK &&
       !CanTemporarilyNavigateHost(url))) {
    ClearObserverState();
  }
  finished_redirects_ = false;
}

void ManagedModeNavigationObserver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (!ShouldStayElevatedForURL(params.url))
    is_elevated_ = false;
  DVLOG(1) << "DidNavigateMainFrame " << params.url.spec();

  // The page already loaded so we are not redirecting anymore, so the
  // next call to ProvisionalChangeToMainFrameURL will probably be after a
  // click.
  // TODO(sergiu): One case where I think this fails is if we get a client-side
  // redirect to a different domain as that will not have a temporary
  // exception. See about that in the future.
  finished_redirects_ = true;

  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(params.url);

  content::RecordAction(UserMetricsAction("ManagedMode_MainFrameNavigation"));
  UMA_HISTOGRAM_ENUMERATION("ManagedMode.FilteringBehavior",
                            behavior,
                            ManagedModeURLFilter::HISTOGRAM_BOUNDING_VALUE);

  if (behavior == ManagedModeURLFilter::ALLOW) {
    // Update the last allowed page so that we can go back to it.
    last_allowed_page_ = web_contents()->GetController().GetCurrentEntryIndex();
  }

  // Record all the URLs this navigation went through.
  if ((behavior == ManagedModeURLFilter::ALLOW &&
       state_ == RECORDING_URLS_AFTER_PREVIEW) ||
      !CanTemporarilyNavigateHost(params.url)) {
    for (std::vector<GURL>::const_iterator it = params.redirects.begin();
         it != params.redirects.end(); ++it) {
      DVLOG(1) << "Adding: " << it->spec();
      AddURLToPatternList(*it);
    }
  }

  if (behavior == ManagedModeURLFilter::ALLOW &&
      state_ == RECORDING_URLS_AFTER_PREVIEW) {
    // The initial page that triggered the interstitial was blocked but the
    // final page is already in the whitelist so add the series of URLs
    // which lead to the final page to the whitelist as well.
    AddSavedURLsToWhitelistAndClearState();
    // This page is now allowed so save the index as well.
    last_allowed_page_ = web_contents()->GetController().GetCurrentEntryIndex();
    SimpleAlertInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents()),
        NULL,
        l10n_util::GetStringUTF16(IDS_MANAGED_MODE_ALREADY_ADDED_MESSAGE),
        true);
    return;
  }

  // Update the exception to the last host visited. A redirect can follow this
  // so don't update the state yet.
  if (state_ == RECORDING_URLS_AFTER_PREVIEW) {
    AddTemporaryException();
  }
}

void ManagedModeNavigationObserver::ProvisionalChangeToMainFrameUrl(
    const GURL& url,
    content::RenderViewHost* render_view_host) {
  if (!ShouldStayElevatedForURL(url))
    is_elevated_ = false;

  // This function is the last one to be called before the resource throttle
  // shows the interstitial if the URL must be blocked.
  DVLOG(1) << "ProvisionalChangeToMainFrameURL " << url.spec();

  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(url);
  if (behavior != ManagedModeURLFilter::BLOCK)
    return;

  if (finished_redirects_ && state_ == RECORDING_URLS_AFTER_PREVIEW &&
      !CanTemporarilyNavigateHost(url))
    ClearObserverState();
}

void ManagedModeNavigationObserver::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  if (!is_main_frame)
    return;

  DVLOG(1) << "DidCommitProvisionalLoadForFrame " << url.spec();
  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(url);

  if (behavior == ManagedModeURLFilter::WARN) {
    if (!warn_infobar_delegate_) {
      warn_infobar_delegate_ = ManagedModeWarningInfobarDelegate::Create(
          InfoBarService::FromWebContents(web_contents()), last_allowed_page_);
    }
  } else {
    if (warn_infobar_delegate_) {
      InfoBarService* infobar_service =
          InfoBarService::FromWebContents(web_contents());
      infobar_service->RemoveInfoBar(warn_infobar_delegate_);
      warn_infobar_delegate_ = NULL;
    }
  }

  if (behavior == ManagedModeURLFilter::BLOCK) {
    DCHECK_EQ(RECORDING_URLS_AFTER_PREVIEW, state_);
    // Add the infobar.
    if (!preview_infobar_delegate_) {
      preview_infobar_delegate_ = ManagedModePreviewInfobarDelegate::Create(
          InfoBarService::FromWebContents(web_contents()));
    }
  }
}

// static
void ManagedModeNavigationObserver::OnRequestBlocked(
    int render_process_host_id,
    int render_view_id,
    const GURL& url,
    const base::Callback<void(bool)>& callback) {
  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, render_view_id);
  if (!web_contents) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(callback, true));
    return;
  }

  ManagedModeNavigationObserver* navigation_observer =
      ManagedModeNavigationObserver::FromWebContents(web_contents);
  if (navigation_observer)
    navigation_observer->SetStateToRecordingAfterPreview();

  // Create a history entry for the attempt and mark it as such.
  history::HistoryAddPageArgs add_page_args(
        web_contents->GetURL(), base::Time::Now(), web_contents, 0,
        web_contents->GetURL(), history::RedirectList(),
        content::PAGE_TRANSITION_BLOCKED, history::SOURCE_BROWSED,
        false);

  // Add the entry to the history database.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HistoryService* history_service =
     HistoryServiceFactory::GetForProfile(profile, Profile::IMPLICIT_ACCESS);

  // |history_service| is null if saving history is disabled.
  if (history_service)
    history_service->AddPage(add_page_args);

  // Show the interstitial.
  new ManagedModeInterstitial(web_contents, url, callback);
}
