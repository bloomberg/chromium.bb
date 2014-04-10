// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/managed_mode/managed_mode_interstitial.h"
#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using base::Time;
using content::NavigationEntry;

namespace {


// Helpers --------------------------------------------------------------------

#if !defined(OS_ANDROID)
// TODO(bauerb): Get rid of the platform-specific #ifdef here.
// http://crbug.com/313377
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
#endif

// ManagedModeWarningInfoBarDelegate ------------------------------------------

class ManagedModeWarningInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a managed mode warning infobar and delegate and adds the infobar to
  // |infobar_service|.  Returns the infobar if it was successfully added.
  static InfoBar* Create(InfoBarService* infobar_service);

 private:
  ManagedModeWarningInfoBarDelegate();
  virtual ~ManagedModeWarningInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual bool ShouldExpire(const NavigationDetails& details) const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeWarningInfoBarDelegate);
};

// static
InfoBar* ManagedModeWarningInfoBarDelegate::Create(
    InfoBarService* infobar_service) {
  return infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new ManagedModeWarningInfoBarDelegate())));
}

ManagedModeWarningInfoBarDelegate::ManagedModeWarningInfoBarDelegate()
    : ConfirmInfoBarDelegate() {
}

ManagedModeWarningInfoBarDelegate::~ManagedModeWarningInfoBarDelegate() {
}

bool ManagedModeWarningInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // ManagedModeNavigationObserver removes us below.
  return false;
}

void ManagedModeWarningInfoBarDelegate::InfoBarDismissed() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  ManagedModeNavigationObserver::FromWebContents(
      web_contents)->WarnInfoBarDismissed();
}

base::string16 ManagedModeWarningInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MANAGED_USER_WARN_INFOBAR_MESSAGE);
}

int ManagedModeWarningInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 ManagedModeWarningInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_MANAGED_USER_WARN_INFOBAR_GO_BACK);
}

bool ManagedModeWarningInfoBarDelegate::Accept() {
#if defined(OS_ANDROID)
  // TODO(bauerb): Get rid of the platform-specific #ifdef here.
  // http://crbug.com/313377
  NOTIMPLEMENTED();
#else
  GoBackToSafety(InfoBarService::WebContentsFromInfoBar(infobar()));
#endif

  return false;
}


}  // namespace

// ManagedModeNavigationObserver ----------------------------------------------

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagedModeNavigationObserver);

ManagedModeNavigationObserver::~ManagedModeNavigationObserver() {
}

ManagedModeNavigationObserver::ManagedModeNavigationObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      warn_infobar_(NULL) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  managed_user_service_ = ManagedUserServiceFactory::GetForProfile(profile);
  url_filter_ = managed_user_service_->GetURLFilterForUIThread();
}

void ManagedModeNavigationObserver::WarnInfoBarDismissed() {
  DCHECK(warn_infobar_);
  warn_infobar_ = NULL;
}

void ManagedModeNavigationObserver::ProvisionalChangeToMainFrameUrl(
    const GURL& url,
    content::RenderFrameHost* render_frame_host) {
  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(url);

  if (behavior == ManagedModeURLFilter::WARN || !warn_infobar_)
    return;

  // If we shouldn't have a warn infobar remove it here.
  InfoBarService::FromWebContents(web_contents())->RemoveInfoBar(warn_infobar_);
  warn_infobar_ = NULL;
}

void ManagedModeNavigationObserver::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    const base::string16& frame_unique_name,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  if (!is_main_frame)
    return;

  DVLOG(1) << "DidCommitProvisionalLoadForFrame " << url.spec();
  ManagedModeURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(url);

  if (behavior == ManagedModeURLFilter::WARN && !warn_infobar_) {
    warn_infobar_ = ManagedModeWarningInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents()));
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
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE, base::Bind(callback, false));
    return;
  }

  ManagedModeNavigationObserver* navigation_observer =
      ManagedModeNavigationObserver::FromWebContents(web_contents);
  if (navigation_observer)
    navigation_observer->OnRequestBlockedInternal(url);

  // Show the interstitial.
  new ManagedModeInterstitial(web_contents, url, callback);
}

void ManagedModeNavigationObserver::OnRequestBlockedInternal(const GURL& url) {
  Time timestamp = Time::Now();  // TODO(bauerb): Use SaneTime when available.
  // Create a history entry for the attempt and mark it as such.
  history::HistoryAddPageArgs add_page_args(
        url, timestamp, web_contents(), 0,
        url, history::RedirectList(),
        content::PAGE_TRANSITION_BLOCKED, history::SOURCE_BROWSED,
        false);

  // Add the entry to the history database.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  HistoryService* history_service =
     HistoryServiceFactory::GetForProfile(profile, Profile::IMPLICIT_ACCESS);

  // |history_service| is null if saving history is disabled.
  if (history_service)
    history_service->AddPage(add_page_args);

  scoped_ptr<NavigationEntry> entry(NavigationEntry::Create());
  entry->SetVirtualURL(url);
  entry->SetTimestamp(timestamp);
  blocked_navigations_.push_back(entry.release());
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);
  managed_user_service->DidBlockNavigation(web_contents());
}
