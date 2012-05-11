// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/alternate_nav_url_fetcher.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/link_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_fetcher.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domain.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::NavigationController;
using content::OpenURLParams;
using content::Referrer;

// AlternateNavInfoBarDelegate ------------------------------------------------

class AlternateNavInfoBarDelegate : public LinkInfoBarDelegate {
 public:
  AlternateNavInfoBarDelegate(InfoBarTabHelper* owner,
                              const GURL& alternate_nav_url);
  virtual ~AlternateNavInfoBarDelegate();

 private:
  // LinkInfoBarDelegate
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageTextWithOffset(size_t* link_offset) const OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  GURL alternate_nav_url_;

  DISALLOW_COPY_AND_ASSIGN(AlternateNavInfoBarDelegate);
};

AlternateNavInfoBarDelegate::AlternateNavInfoBarDelegate(
    InfoBarTabHelper* owner,
    const GURL& alternate_nav_url)
    : LinkInfoBarDelegate(owner),
      alternate_nav_url_(alternate_nav_url) {
}

AlternateNavInfoBarDelegate::~AlternateNavInfoBarDelegate() {
}

gfx::Image* AlternateNavInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_ALT_NAV_URL);
}

InfoBarDelegate::Type AlternateNavInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 AlternateNavInfoBarDelegate::GetMessageTextWithOffset(
    size_t* link_offset) const {
  const string16 label = l10n_util::GetStringFUTF16(
      IDS_ALTERNATE_NAV_URL_VIEW_LABEL, string16(), link_offset);
  return label;
}

string16 AlternateNavInfoBarDelegate::GetLinkText() const {
  return UTF8ToUTF16(alternate_nav_url_.spec());
}

bool AlternateNavInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  OpenURLParams params(
      alternate_nav_url_, Referrer(), disposition,
      // Pretend the user typed this URL, so that navigating to
      // it will be the default action when it's typed again in
      // the future.
      content::PAGE_TRANSITION_TYPED,
      false);
  owner()->web_contents()->OpenURL(params);

  // We should always close, even if the navigation did not occur within this
  // WebContents.
  return true;
}


// AlternateNavURLFetcher -----------------------------------------------------

AlternateNavURLFetcher::AlternateNavURLFetcher(
    const GURL& alternate_nav_url)
    : alternate_nav_url_(alternate_nav_url),
      controller_(NULL),
      state_(NOT_STARTED),
      navigated_to_entry_(false) {
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_INSTANT_COMMITTED,
                 content::NotificationService::AllSources());
}

AlternateNavURLFetcher::~AlternateNavURLFetcher() {
}

void AlternateNavURLFetcher::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_PENDING: {
      // If we've already received a notification for the same controller, we
      // should delete ourselves as that indicates that the page is being
      // re-loaded so this instance is now stale.
      NavigationController* controller =
          content::Source<NavigationController>(source).ptr();
      if (controller_ == controller) {
        delete this;
      } else if (!controller_) {
        // Start listening for the commit notification.
        DCHECK(controller->GetPendingEntry());
        registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                       content::Source<NavigationController>(
                          controller));
        StartFetch(controller);
      }
      break;
    }

    case chrome::NOTIFICATION_INSTANT_COMMITTED: {
      // See above.
      NavigationController* controller =
          &content::Source<TabContentsWrapper>(source)->
              web_contents()->GetController();
      if (controller_ == controller) {
        delete this;
      } else if (!controller_) {
        navigated_to_entry_ = true;
        StartFetch(controller);
      }
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_COMMITTED:
      // The page was navigated, we can show the infobar now if necessary.
      registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                        content::Source<NavigationController>(controller_));
      navigated_to_entry_ = true;
      ShowInfobarIfPossible();
      // WARNING: |this| may be deleted!
      break;

    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED:
      // We have been closed. In order to prevent the URLFetcher from trying to
      // access the controller that will be invalid, we delete ourselves.
      // This deletes the URLFetcher and insures its callback won't be called.
      delete this;
      break;

    default:
      NOTREACHED();
  }
}

void AlternateNavURLFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK_EQ(fetcher_.get(), source);
  SetStatusFromURLFetch(
      source->GetURL(), source->GetStatus(), source->GetResponseCode());
  ShowInfobarIfPossible();
  // WARNING: |this| may be deleted!
}

void AlternateNavURLFetcher::StartFetch(NavigationController* controller) {
  controller_ = controller;
  registrar_.Add(
      this,
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(controller_->GetWebContents()));

  DCHECK_EQ(NOT_STARTED, state_);
  state_ = IN_PROGRESS;
  fetcher_.reset(content::URLFetcher::Create(
      GURL(alternate_nav_url_), content::URLFetcher::HEAD, this));
  fetcher_->SetRequestContext(
      controller_->GetBrowserContext()->GetRequestContext());

  content::WebContents* web_contents = controller_->GetWebContents();
  fetcher_->AssociateWithRenderView(
      web_contents->GetURL(),
      web_contents->GetRenderProcessHost()->GetID(),
      web_contents->GetRenderViewHost()->GetRoutingID());
  fetcher_->Start();
}

void AlternateNavURLFetcher::SetStatusFromURLFetch(
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code) {
  if (!status.is_success() ||
      // HTTP 2xx, 401, and 407 all indicate that the target address exists.
      (((response_code / 100) != 2) &&
       (response_code != 401) && (response_code != 407)) ||
      // Fail if we're redirected to a common location.
      // This happens for ISPs/DNS providers/etc. who return
      // provider-controlled pages to arbitrary user navigation attempts.
      // Because this can result in infobars on large fractions of user
      // searches, we don't show automatic infobars for these.  Note that users
      // can still choose to explicitly navigate to or search for pages in
      // these domains, and can still get infobars for cases that wind up on
      // other domains (e.g. legit intranet sites), we're just trying to avoid
      // erroneously harassing the user with our own UI prompts.
      net::RegistryControlledDomainService::SameDomainOrHost(url,
          IntranetRedirectDetector::RedirectOrigin())) {
    state_ = FAILED;
    return;
  }
  state_ = SUCCEEDED;
}

void AlternateNavURLFetcher::ShowInfobarIfPossible() {
  if (!navigated_to_entry_ || state_ != SUCCEEDED) {
    if (state_ == FAILED)
      delete this;
    return;
  }

  InfoBarTabHelper* infobar_helper =
      TabContentsWrapper::GetCurrentWrapperForContents(
          controller_->GetWebContents())->infobar_tab_helper();
  infobar_helper->AddInfoBar(
      new AlternateNavInfoBarDelegate(infobar_helper, alternate_nav_url_));
  delete this;
}
