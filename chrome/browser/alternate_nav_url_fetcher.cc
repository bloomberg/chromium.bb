// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/alternate_nav_url_fetcher.h"

#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/infobars/alternate_nav_infobar_delegate.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"

using content::NavigationController;

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
          &content::Source<content::WebContents>(source)->GetController();
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
  content::WebContents* web_contents = controller_->GetWebContents();
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<content::WebContents>(web_contents));

  DCHECK_EQ(NOT_STARTED, state_);
  state_ = IN_PROGRESS;
  fetcher_.reset(net::URLFetcher::Create(GURL(alternate_nav_url_),
                                         net::URLFetcher::HEAD, this));
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetRequestContext(
      controller_->GetBrowserContext()->GetRequestContext());
  fetcher_->SetStopOnRedirect(true);
  fetcher_->Start();
}

void AlternateNavURLFetcher::SetStatusFromURLFetch(
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success()) {
    // HTTP 2xx, 401, and 407 all indicate that the target address exists.
    state_ = (((response_code / 100) == 2) || (response_code == 401) ||
        (response_code == 407)) ? SUCCEEDED : FAILED;
  } else {
    // If we got HTTP 3xx, we'll have auto-canceled; treat this as evidence the
    // target address exists as long as we're not redirected to a common
    // location every time, lest we annoy the user with infobars on everything
    // they type.  See comments on IntranetRedirectDetector.
    state_ = ((status.status() == net::URLRequestStatus::CANCELED) &&
      ((response_code / 100) == 3) &&
      !net::RegistryControlledDomainService::SameDomainOrHost(url,
          IntranetRedirectDetector::RedirectOrigin())) ? SUCCEEDED : FAILED;
  }
}

void AlternateNavURLFetcher::ShowInfobarIfPossible() {
  if (navigated_to_entry_ && (state_ == SUCCEEDED)) {
    AlternateNavInfoBarDelegate::Create(
        InfoBarService::FromWebContents(controller_->GetWebContents()),
        alternate_nav_url_);
  } else if (state_ != FAILED) {
    return;
  }
  delete this;
}
