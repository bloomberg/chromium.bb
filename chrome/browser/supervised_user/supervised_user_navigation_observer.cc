// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_resource_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/history/core/browser/history_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
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
      chrome::GetHostDesktopTypeForNativeView(web_contents->GetNativeView());
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

// SupervisedUserWarningInfoBarDelegate ---------------------------------------

class SupervisedUserWarningInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a supervised user warning infobar and delegate and adds the infobar
  // to |infobar_service|.  Returns the infobar if it was successfully added.
  static infobars::InfoBar* Create(InfoBarService* infobar_service);

 private:
  SupervisedUserWarningInfoBarDelegate();
  virtual ~SupervisedUserWarningInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual bool ShouldExpire(const NavigationDetails& details) const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserWarningInfoBarDelegate);
};

// static
infobars::InfoBar* SupervisedUserWarningInfoBarDelegate::Create(
    InfoBarService* infobar_service) {
  return infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new SupervisedUserWarningInfoBarDelegate())));
}

SupervisedUserWarningInfoBarDelegate::SupervisedUserWarningInfoBarDelegate()
    : ConfirmInfoBarDelegate() {
}

SupervisedUserWarningInfoBarDelegate::~SupervisedUserWarningInfoBarDelegate() {
}

bool SupervisedUserWarningInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // SupervisedUserNavigationObserver removes us below.
  return false;
}

void SupervisedUserWarningInfoBarDelegate::InfoBarDismissed() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  SupervisedUserNavigationObserver::FromWebContents(
      web_contents)->WarnInfoBarDismissed();
}

base::string16 SupervisedUserWarningInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_SUPERVISED_USER_WARN_INFOBAR_MESSAGE);
}

int SupervisedUserWarningInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 SupervisedUserWarningInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_SUPERVISED_USER_WARN_INFOBAR_GO_BACK);
}

bool SupervisedUserWarningInfoBarDelegate::Accept() {
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

// SupervisedUserNavigationObserver -------------------------------------------

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SupervisedUserNavigationObserver);

SupervisedUserNavigationObserver::~SupervisedUserNavigationObserver() {
}

SupervisedUserNavigationObserver::SupervisedUserNavigationObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      warn_infobar_(NULL) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  supervised_user_service_ =
      SupervisedUserServiceFactory::GetForProfile(profile);
  url_filter_ = supervised_user_service_->GetURLFilterForUIThread();
}

void SupervisedUserNavigationObserver::WarnInfoBarDismissed() {
  DCHECK(warn_infobar_);
  warn_infobar_ = NULL;
}

void SupervisedUserNavigationObserver::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  if (render_frame_host->GetParent())
    return;

  DVLOG(1) << "DidCommitProvisionalLoadForFrame " << url.spec();
  SupervisedUserURLFilter::FilteringBehavior behavior =
      url_filter_->GetFilteringBehaviorForURL(url);

  if (behavior == SupervisedUserURLFilter::WARN && !warn_infobar_) {
    warn_infobar_ = SupervisedUserWarningInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents()));
  } else if (behavior != SupervisedUserURLFilter::WARN && warn_infobar_) {
    InfoBarService::FromWebContents(web_contents())->
        RemoveInfoBar(warn_infobar_);
    warn_infobar_ = NULL;
  }
}

// static
void SupervisedUserNavigationObserver::OnRequestBlocked(
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

  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents);
  if (navigation_observer)
    navigation_observer->OnRequestBlockedInternal(url);

  // Show the interstitial.
  SupervisedUserInterstitial::Show(web_contents, url, callback);
}

void SupervisedUserNavigationObserver::OnRequestBlockedInternal(
    const GURL& url) {
  Time timestamp = Time::Now();  // TODO(bauerb): Use SaneTime when available.
  // Create a history entry for the attempt and mark it as such.
  history::HistoryAddPageArgs add_page_args(
        url, timestamp, web_contents(), 0,
        url, history::RedirectList(),
        ui::PAGE_TRANSITION_BLOCKED, history::SOURCE_BROWSED,
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
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  supervised_user_service->DidBlockNavigation(web_contents());
}
