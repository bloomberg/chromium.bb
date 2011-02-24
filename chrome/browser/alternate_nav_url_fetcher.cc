// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/alternate_nav_url_fetcher.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/registry_controlled_domain.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

AlternateNavURLFetcher::AlternateNavURLFetcher(
    const GURL& alternate_nav_url)
    : LinkInfoBarDelegate(NULL),
      alternate_nav_url_(alternate_nav_url),
      controller_(NULL),
      state_(NOT_STARTED),
      navigated_to_entry_(false),
      infobar_contents_(NULL) {
  registrar_.Add(this, NotificationType::NAV_ENTRY_PENDING,
                 NotificationService::AllSources());
}

AlternateNavURLFetcher::~AlternateNavURLFetcher() {}

void AlternateNavURLFetcher::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::NAV_ENTRY_PENDING:
      // If we've already received a notification for the same controller, we
      // should delete ourselves as that indicates that the page is being
      // re-loaded so this instance is now stale.
      // http://crbug.com/43378
      if (!infobar_contents_ &&
          controller_ == Source<NavigationController>(source).ptr()) {
        delete this;
      } else if (!controller_) {
        controller_ = Source<NavigationController>(source).ptr();
        DCHECK(controller_->pending_entry());

        // Start listening for the commit notification. We also need to listen
        // for the tab close command since that means the load will never
        // commit!
        registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                       Source<NavigationController>(controller_));
        registrar_.Add(this, NotificationType::TAB_CLOSED,
                       Source<NavigationController>(controller_));

        DCHECK_EQ(NOT_STARTED, state_);
        state_ = IN_PROGRESS;
        fetcher_.reset(new URLFetcher(GURL(alternate_nav_url_),
                                      URLFetcher::HEAD, this));
        fetcher_->set_request_context(
            controller_->profile()->GetRequestContext());
        fetcher_->Start();
      }
      break;

    case NotificationType::NAV_ENTRY_COMMITTED:
      // The page was navigated, we can show the infobar now if necessary.
      registrar_.Remove(this, NotificationType::NAV_ENTRY_COMMITTED,
                        Source<NavigationController>(controller_));
      navigated_to_entry_ = true;
      ShowInfobarIfPossible();
      break;

    case NotificationType::TAB_CLOSED:
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
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  DCHECK_EQ(fetcher_.get(), source);
  SetStatusFromURLFetch(url, status, response_code);
  ShowInfobarIfPossible();
}

SkBitmap* AlternateNavURLFetcher::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_ALT_NAV_URL);
}

InfoBarDelegate::Type AlternateNavURLFetcher::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 AlternateNavURLFetcher::GetMessageTextWithOffset(
    size_t* link_offset) const {
  const string16 label = l10n_util::GetStringFUTF16(
      IDS_ALTERNATE_NAV_URL_VIEW_LABEL, string16(), link_offset);
  return label;
}

string16 AlternateNavURLFetcher::GetLinkText() const {
  return UTF8ToUTF16(alternate_nav_url_.spec());
}

bool AlternateNavURLFetcher::LinkClicked(WindowOpenDisposition disposition) {
  infobar_contents_->OpenURL(
      alternate_nav_url_, GURL(), disposition,
      // Pretend the user typed this URL, so that navigating to
      // it will be the default action when it's typed again in
      // the future.
      PageTransition::TYPED);

  // We should always close, even if the navigation did not occur within this
  // TabContents.
  return true;
}

void AlternateNavURLFetcher::InfoBarClosed() {
  delete this;
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

  infobar_contents_ = controller_->tab_contents();
  StoreActiveEntryUniqueID(infobar_contents_);
  infobar_contents_->AddInfoBar(this);
}
