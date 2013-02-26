// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/api/infobars/simple_alert_infobar_delegate.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/managed_mode/managed_mode_interstitial.h"
#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

// For use in histograms.
enum PreviewInfobarCommand {
  INFOBAR_ACCEPT,
  INFOBAR_CANCEL,
  INFOBAR_HISTOGRAM_BOUNDING_VALUE
};

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
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // InfoBarDelegate override:
  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;

  int last_allowed_page_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeWarningInfobarDelegate);
};

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
  GoBackToSafety(owner()->GetWebContents());

  return false;
}

bool ManagedModeWarningInfobarDelegate::Cancel() {
  NOTREACHED();
  return false;
}

bool ManagedModeWarningInfobarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  // ManagedModeNavigationObserver removes us below.
  return false;
}

void ManagedModeWarningInfobarDelegate::InfoBarDismissed() {
  ManagedModeNavigationObserver* observer =
      ManagedModeNavigationObserver::FromWebContents(owner()->GetWebContents());
  observer->WarnInfobarDismissed();
}

class ManagedModePreviewInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a managed mode preview delegate and adds it to |infobar_service|.
  // Returns the delegate if it was successfully added.
  static InfoBarDelegate* Create(InfoBarService* infobar_service);

 private:
  explicit ManagedModePreviewInfobarDelegate(InfoBarService* infobar_service);
  virtual ~ManagedModePreviewInfobarDelegate();

  // ConfirmInfoBarDelegate overrides:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // InfoBarDelegate override:
  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;

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
      ManagedModeNavigationObserver::FromWebContents(
          owner()->GetWebContents());
  UMA_HISTOGRAM_ENUMERATION("ManagedMode.PreviewInfobarCommand",
                            INFOBAR_ACCEPT,
                            INFOBAR_HISTOGRAM_BOUNDING_VALUE);
  observer->AddSavedURLsToWhitelistAndClearState();
  return true;
}

bool ManagedModePreviewInfobarDelegate::Cancel() {
  UMA_HISTOGRAM_ENUMERATION("ManagedMode.PreviewInfobarCommand",
                            INFOBAR_CANCEL,
                            INFOBAR_HISTOGRAM_BOUNDING_VALUE);
  GoBackToSafety(owner()->GetWebContents());
  return false;
}

bool ManagedModePreviewInfobarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  // ManagedModeNavigationObserver removes us below.
  return false;
}

void ManagedModePreviewInfobarDelegate::InfoBarDismissed() {
  ManagedModeNavigationObserver* observer =
      ManagedModeNavigationObserver::FromWebContents(
          owner()->GetWebContents());
  observer->PreviewInfobarDismissed();
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagedModeNavigationObserver);

ManagedModeNavigationObserver::~ManagedModeNavigationObserver() {
  RemoveTemporaryException();
}

ManagedModeNavigationObserver::ManagedModeNavigationObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      warn_infobar_delegate_(NULL),
      preview_infobar_delegate_(NULL),
      got_user_gesture_(false),
      state_(RECORDING_URLS_BEFORE_PREVIEW),
      last_allowed_page_(-1) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  managed_user_service_ = ManagedUserServiceFactory::GetForProfile(profile);
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
                 last_url_,
                 got_user_gesture_));
}

void ManagedModeNavigationObserver::UpdateExceptionNavigationStatus() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(web_contents());

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeResourceThrottle::UpdateExceptionNavigationStatus,
                 web_contents()->GetRenderProcessHost()->GetID(),
                 web_contents()->GetRenderViewHost()->GetRoutingID(),
                 got_user_gesture_));
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
  if (web_contents()->GetController().GetCurrentEntryIndex() <
      last_allowed_page_) {
    ClearObserverState();
  }
}

void ManagedModeNavigationObserver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {

  content::RecordAction(UserMetricsAction("ManagedMode_MainFrameNavigation"));

  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(params.url);

  UMA_HISTOGRAM_ENUMERATION("ManagedMode.FilteringBehavior",
                            behavior,
                            ManagedModeURLFilter::HISTOGRAM_BOUNDING_VALUE);

  // The page can be redirected to a different domain, record those URLs as
  // well.
  if (behavior == ManagedModeURLFilter::BLOCK &&
      !CanTemporarilyNavigateHost(params.url))
    AddURLToPatternList(params.url);

  if (behavior == ManagedModeURLFilter::ALLOW &&
      state_ == RECORDING_URLS_AFTER_PREVIEW) {
    // The initial page that triggered the interstitial was blocked but the
    // final page is already in the whitelist so add the series of URLs
    // which lead to the final page to the whitelist as well.
    // Update the |last_url_| since it was not added to the list before.
    last_url_ = params.url;
    AddSavedURLsToWhitelistAndClearState();
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

  // The navigation is complete, unless there is a redirect. So set the
  // new navigation to false to detect user interaction.
  got_user_gesture_ = false;
}

void ManagedModeNavigationObserver::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    int64 parent_frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc,
    content::RenderViewHost* render_view_host) {
  if (!is_main_frame)
    return;
}

void ManagedModeNavigationObserver::ProvisionalChangeToMainFrameUrl(
    const GURL& url,
    content::RenderViewHost* render_view_host) {
  // This function is the last one to be called before the resource throttle
  // shows the interstitial if the URL must be blocked.
  DVLOG(1) << "ProvisionalChangeToMainFrameURL " << url.spec();
  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(url);

  if (behavior != ManagedModeURLFilter::BLOCK)
    return;

  if (state_ == RECORDING_URLS_AFTER_PREVIEW && got_user_gesture_ &&
      !CanTemporarilyNavigateHost(url))
    ClearObserverState();

  if (behavior == ManagedModeURLFilter::BLOCK &&
      !CanTemporarilyNavigateHost(url))
    AddURLToPatternList(url);

  got_user_gesture_ = false;
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

  if (behavior == ManagedModeURLFilter::ALLOW)
    last_allowed_page_ = web_contents()->GetController().GetCurrentEntryIndex();
}

void ManagedModeNavigationObserver::DidGetUserGesture() {
  got_user_gesture_ = true;
  // Update the exception status so that the resource throttle knows that
  // there was a manual navigation.
  UpdateExceptionNavigationStatus();
}
