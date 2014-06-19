// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/chrome_google_url_tracker_client.h"

#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_url_tracker_navigation_helper_impl.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

ChromeGoogleURLTrackerClient::ChromeGoogleURLTrackerClient(Profile* profile)
    : profile_(profile) {
}

ChromeGoogleURLTrackerClient::~ChromeGoogleURLTrackerClient() {
}

void ChromeGoogleURLTrackerClient::SetListeningForNavigationStart(bool listen) {
  if (listen) {
    registrar_.Add(
        this,
        content::NOTIFICATION_NAV_ENTRY_PENDING,
        content::NotificationService::AllBrowserContextsAndSources());
  } else {
    registrar_.Remove(
        this,
        content::NOTIFICATION_NAV_ENTRY_PENDING,
        content::NotificationService::AllBrowserContextsAndSources());
  }
}

bool ChromeGoogleURLTrackerClient::IsListeningForNavigationStart() {
  return registrar_.IsRegistered(
      this,
      content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::NotificationService::AllBrowserContextsAndSources());
}

bool ChromeGoogleURLTrackerClient::IsBackgroundNetworkingEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableBackgroundNetworking);
}

PrefService* ChromeGoogleURLTrackerClient::GetPrefs() {
  return profile_->GetPrefs();
}

net::URLRequestContextGetter*
ChromeGoogleURLTrackerClient::GetRequestContext() {
  return profile_->GetRequestContext();
}

void ChromeGoogleURLTrackerClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_PENDING, type);
  content::NavigationController* controller =
      content::Source<content::NavigationController>(source).ptr();
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(controller->GetWebContents());
  // Because we're listening to all sources, there may be no InfoBarService for
  // some notifications, e.g. navigations in bubbles/balloons etc.
  if (infobar_service) {
    google_url_tracker()->OnNavigationPending(
        scoped_ptr<GoogleURLTrackerNavigationHelper>(
            new GoogleURLTrackerNavigationHelperImpl(
                controller->GetWebContents(), google_url_tracker())),
        infobar_service,
        controller->GetPendingEntry()->GetUniqueID());
  }
}
