// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/api/infobars/simple_alert_infobar_delegate.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/managed_mode/managed_mode_interstitial.h"
#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/frame_navigate_params.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

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
  // If this is the last tab, open a new window.
  if (BrowserList::size() == 1) {
    Browser* browser = *(BrowserList::begin());
    DCHECK(browser == chrome::FindBrowserWithWebContents(web_contents));
    if (browser->tab_count() == 1)
      chrome::NewEmptyWindow(browser->profile());
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
  observer->AddSavedURLsToWhitelistAndClearState();
  return true;
}

bool ManagedModePreviewInfobarDelegate::Cancel() {
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
      url_filter_(ManagedMode::GetURLFilterForUIThread()),
      warn_infobar_delegate_(NULL),
      preview_infobar_delegate_(NULL),
      state_(RECORDING_URLS_BEFORE_PREVIEW),
      last_allowed_page_(-1) {}

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
  ListValue whitelist;
  for (std::set<GURL>::const_iterator it = navigated_urls_.begin();
       it != navigated_urls_.end();
       ++it) {
    whitelist.AppendString(it->scheme() + "://." + it->host() + it->path());
  }
  if (last_url_.is_valid()) {
    if (last_url_.SchemeIs("https")) {
      whitelist.AppendString("https://" + last_url_.host());
    } else {
      whitelist.AppendString(last_url_.host());
    }
  }
  ManagedMode::AddToManualList(true, whitelist);
  ClearObserverState();
}

void ManagedModeNavigationObserver::AddURLToPatternList(const GURL& url) {
  DCHECK(state_ != NOT_RECORDING_URLS);
  navigated_urls_.insert(url);
}

void ManagedModeNavigationObserver::AddURLAsLastPattern(const GURL& url) {
  DCHECK(state_ != NOT_RECORDING_URLS);

  // Erase the last |url| if it is present in the |navigated_urls_|. This stops
  // us from having both http://.www.google.com (exact URL from the pattern
  // list) and www.google.com (hostname from the last URL pattern) in the list.
  navigated_urls_.erase(url);
  last_url_ = url;
}

void ManagedModeNavigationObserver::SetStateToRecordingAfterPreview() {
  state_ = RECORDING_URLS_AFTER_PREVIEW;
}

bool ManagedModeNavigationObserver::CanTemporarilyNavigateHost(
    const GURL& url) {
  if (last_url_.scheme() == "https") {
    return url.scheme() == "https" && last_url_.host() == url.host();
  }
  return last_url_.host() == url.host();
}

void ManagedModeNavigationObserver::ClearObserverState() {
  if (state_ == NOT_RECORDING_URLS && preview_infobar_delegate_) {
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents());
    infobar_service->RemoveInfoBar(preview_infobar_delegate_);
    preview_infobar_delegate_ = NULL;
  }
  navigated_urls_.clear();
  last_url_ = GURL();
  state_ = RECORDING_URLS_BEFORE_PREVIEW;
  RemoveTemporaryException();
}

void ManagedModeNavigationObserver::NavigateToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {

  // This method gets called first when a user navigates to a (new) URL.
  // This means that the data related to the list of URLs needs to be cleared
  // in certain circumstances.
  if (web_contents()->GetController().GetCurrentEntryIndex() <
          last_allowed_page_ || !CanTemporarilyNavigateHost(url)) {
    ClearObserverState();
  }
}

void ManagedModeNavigationObserver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {

  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(params.url);

  // If the user just saw an interstitial this is the final URL so it is
  // recorded. Checking for filtering behavior here isn't useful because
  // although this specific URL can be allowed the hostname will be added which
  // is more general. The hostname will be checked later when it is
  // added to the actual whitelist to see if it is already present.
  if (behavior == ManagedModeURLFilter::BLOCK && state_ != NOT_RECORDING_URLS)
    AddURLAsLastPattern(params.url);

  if (behavior == ManagedModeURLFilter::ALLOW &&
      state_ != RECORDING_URLS_BEFORE_PREVIEW) {
    // The initial page that triggered the interstitial was blocked but the
    // final page is already in the whitelist so add the series of URLs
    // which lead to the final page to the whitelist as well.
    AddSavedURLsToWhitelistAndClearState();
    SimpleAlertInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents()),
        NULL,
        l10n_util::GetStringUTF16(IDS_MANAGED_MODE_ALREADY_ADDED_MESSAGE),
        true);
    return;
  }

  if (state_ == RECORDING_URLS_AFTER_PREVIEW) {
    // A temporary exception should be added only if an interstitial was shown,
    // the user clicked preview and the final page was not allowed. This
    // temporary exception stops the interstitial from showing on further
    // navigations to that host so that the user can navigate around to
    // inspect it.
    state_ = NOT_RECORDING_URLS;
    AddTemporaryException();
  }
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
  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(url);

  if (state_ == NOT_RECORDING_URLS && !CanTemporarilyNavigateHost(url))
    ClearObserverState();

  if (behavior == ManagedModeURLFilter::BLOCK && state_ != NOT_RECORDING_URLS)
    AddURLToPatternList(url);
}

void ManagedModeNavigationObserver::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  if (!is_main_frame)
    return;

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
    switch (state_) {
      case RECORDING_URLS_BEFORE_PREVIEW:
        // Should not be in this state with a blocked URL.
        NOTREACHED();
        break;
      case RECORDING_URLS_AFTER_PREVIEW:
        // Add the infobar.
        if (!preview_infobar_delegate_) {
          preview_infobar_delegate_ =
              ManagedModePreviewInfobarDelegate::Create(
                  InfoBarService::FromWebContents(web_contents()));
        }
        break;
      case NOT_RECORDING_URLS:
        // Check that the infobar is present.
        DCHECK(preview_infobar_delegate_);
        break;
    }
  }

  if (behavior == ManagedModeURLFilter::ALLOW)
    last_allowed_page_ = web_contents()->GetController().GetCurrentEntryIndex();
}
