// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker_navigation_helper_impl.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

GoogleURLTrackerNavigationHelperImpl::GoogleURLTrackerNavigationHelperImpl(
    content::WebContents* web_contents,
    GoogleURLTracker* tracker)
    : GoogleURLTrackerNavigationHelper(tracker),
      web_contents_(web_contents) {
}

GoogleURLTrackerNavigationHelperImpl::~GoogleURLTrackerNavigationHelperImpl() {
  web_contents_ = NULL;
}

void GoogleURLTrackerNavigationHelperImpl::SetListeningForNavigationCommit(
    bool listen) {
  content::NotificationSource navigation_controller_source =
      content::Source<content::NavigationController>(
          &web_contents_->GetController());
  if (listen) {
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                    navigation_controller_source);
  } else {
    registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                      navigation_controller_source);
  }
}

bool GoogleURLTrackerNavigationHelperImpl::IsListeningForNavigationCommit() {
  content::NotificationSource navigation_controller_source =
      content::Source<content::NavigationController>(
          &web_contents_->GetController());
  return registrar_.IsRegistered(
      this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      navigation_controller_source);
}

void GoogleURLTrackerNavigationHelperImpl::SetListeningForTabDestruction(
    bool listen) {
  content::NotificationSource web_contents_source =
      content::Source<content::WebContents>(web_contents_);
  if (listen) {
    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   web_contents_source);
  } else {
    registrar_.Remove(this,
                      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                      web_contents_source);
  }
}

bool GoogleURLTrackerNavigationHelperImpl::IsListeningForTabDestruction() {
  return registrar_.IsRegistered(
      this,
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(web_contents_));
}

void GoogleURLTrackerNavigationHelperImpl::OpenURL(
    GURL url,
    WindowOpenDisposition disposition,
    bool user_clicked_on_link) {
  ui::PageTransition transition_type = user_clicked_on_link ?
      ui::PAGE_TRANSITION_LINK : ui::PAGE_TRANSITION_GENERATED;
  web_contents_->OpenURL(content::OpenURLParams(
      url, content::Referrer(), disposition, transition_type, false));
}

void GoogleURLTrackerNavigationHelperImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      content::NavigationController* controller =
          content::Source<content::NavigationController>(source).ptr();
      DCHECK_EQ(web_contents_, controller->GetWebContents());

      // Here we're only listening to notifications where we already know
      // there's an associated InfoBarService.
      InfoBarService* infobar_service =
          InfoBarService::FromWebContents(web_contents_);
      DCHECK(infobar_service);
      const GURL& search_url = controller->GetActiveEntry()->GetURL();
      if (!search_url.is_valid())  // Not clear if this can happen.
        google_url_tracker()->OnTabClosed(this);
      google_url_tracker()->OnNavigationCommitted(infobar_service, search_url);
      break;
    }

    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      DCHECK_EQ(web_contents_,
                content::Source<content::WebContents>(source).ptr());
      google_url_tracker()->OnTabClosed(this);
      break;
    }

    default:
      NOTREACHED() << "Unknown notification received:" << type;
  }
}
