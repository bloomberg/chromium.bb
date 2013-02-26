// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker_navigation_helper_impl.h"

#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

GoogleURLTrackerNavigationHelperImpl::
    GoogleURLTrackerNavigationHelperImpl() : tracker_(NULL) {
}

GoogleURLTrackerNavigationHelperImpl::
    ~GoogleURLTrackerNavigationHelperImpl() {
}

void GoogleURLTrackerNavigationHelperImpl::SetGoogleURLTracker(
    GoogleURLTracker* tracker) {
  DCHECK(tracker);
  tracker_ = tracker;
}

void GoogleURLTrackerNavigationHelperImpl::SetListeningForNavigationStart(
    bool listen) {
  if (listen) {
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
        content::NotificationService::AllBrowserContextsAndSources());
    registrar_.Add(this, chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::NotificationService::AllBrowserContextsAndSources());
  } else {
    registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
        content::NotificationService::AllBrowserContextsAndSources());
    registrar_.Remove(this, chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::NotificationService::AllBrowserContextsAndSources());
  }
}

bool GoogleURLTrackerNavigationHelperImpl::IsListeningForNavigationStart() {
  return registrar_.IsRegistered(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::NotificationService::AllBrowserContextsAndSources());
}

void GoogleURLTrackerNavigationHelperImpl::SetListeningForNavigationCommit(
    const content::NavigationController* nav_controller,
    bool listen) {
  content::NotificationSource navigation_controller_source =
      content::Source<content::NavigationController>(nav_controller);
  if (listen) {
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                    navigation_controller_source);
  } else {
    registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                      navigation_controller_source);
  }
}

bool GoogleURLTrackerNavigationHelperImpl::IsListeningForNavigationCommit(
    const content::NavigationController* nav_controller) {
  content::NotificationSource navigation_controller_source =
      content::Source<content::NavigationController>(nav_controller);
  return registrar_.IsRegistered(
      this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      navigation_controller_source);
}

void GoogleURLTrackerNavigationHelperImpl::SetListeningForTabDestruction(
    const content::NavigationController* nav_controller,
    bool listen) {
  content::NotificationSource navigation_controller_source =
      content::Source<content::NavigationController>(nav_controller);
  if (listen) {
    registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   GetWebContentsSource(navigation_controller_source));
  } else {
    registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                      GetWebContentsSource(navigation_controller_source));
  }
}

bool GoogleURLTrackerNavigationHelperImpl::IsListeningForTabDestruction(
    const content::NavigationController* nav_controller) {
  content::NotificationSource navigation_controller_source =
      content::Source<content::NavigationController>(nav_controller);
  return registrar_.IsRegistered(
      this,
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      GetWebContentsSource(navigation_controller_source));
}

content::NotificationSource
    GoogleURLTrackerNavigationHelperImpl::GetWebContentsSource(
        const content::NotificationSource& nav_controller_source) {
  content::NavigationController* controller =
      content::Source<content::NavigationController>(
          nav_controller_source).ptr();
  content::WebContents* web_contents = controller->GetWebContents();
  return content::Source<content::WebContents>(web_contents);
}

void GoogleURLTrackerNavigationHelperImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_PENDING: {
      content::NavigationController* controller =
          content::Source<content::NavigationController>(source).ptr();
      content::WebContents* web_contents = controller->GetWebContents();
      InfoBarService* infobar_service =
          InfoBarService::FromWebContents(web_contents);
      // Because we're listening to all sources, there may be no
      // InfoBarService for some notifications, e.g. navigations in
      // bubbles/balloons etc.
      if (infobar_service) {
        tracker_->OnNavigationPending(
            controller, infobar_service,
            controller->GetPendingEntry()->GetUniqueID());
      }
      break;
    }

    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      content::NavigationController* controller =
          content::Source<content::NavigationController>(source).ptr();
      // Here we're only listening to notifications where we already know
      // there's an associated InfoBarService.
      content::WebContents* web_contents = controller->GetWebContents();
      InfoBarService* infobar_service =
          InfoBarService::FromWebContents(web_contents);
      DCHECK(infobar_service);
      const GURL& search_url = controller->GetActiveEntry()->GetURL();
      if (!search_url.is_valid())  // Not clear if this can happen.
        tracker_->OnTabClosed(controller);
      tracker_->OnNavigationCommitted(infobar_service, search_url);
      break;
    }

    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      content::WebContents* web_contents =
          content::Source<content::WebContents>(source).ptr();
      tracker_->OnTabClosed(&web_contents->GetController());
      break;
    }

    case chrome::NOTIFICATION_INSTANT_COMMITTED: {
      content::WebContents* web_contents =
          content::Source<content::WebContents>(source).ptr();
      content::NavigationController* nav_controller =
          &web_contents->GetController();
      const GURL& search_url = web_contents->GetURL();
      if (!search_url.is_valid())  // Not clear if this can happen.
        tracker_->OnTabClosed(nav_controller);
      OnInstantCommitted(nav_controller,
                         InfoBarService::FromWebContents(web_contents),
                         search_url);
      break;
    }

    default:
      NOTREACHED() << "Unknown notification received:" << type;
  }
}

void GoogleURLTrackerNavigationHelperImpl::OnInstantCommitted(
    content::NavigationController* nav_controller,
    InfoBarService* infobar_service,
    const GURL& search_url) {
  // Call OnNavigationPending, giving |tracker_| the option to register for
  // navigation commit messages for this navigation controller. If it does
  // register for them, simulate the commit as well.
  tracker_->OnNavigationPending(nav_controller, infobar_service, 0);
  if (IsListeningForNavigationCommit(nav_controller))
    tracker_->OnNavigationCommitted(infobar_service, search_url);
}
