// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker_map_entry.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_url_tracker_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"


GoogleURLTrackerMapEntry::GoogleURLTrackerMapEntry(
    GoogleURLTracker* google_url_tracker,
    InfoBarService* infobar_service,
    const content::NavigationController* navigation_controller)
    : google_url_tracker_(google_url_tracker),
      infobar_service_(infobar_service),
      infobar_delegate_(NULL),
      navigation_controller_(navigation_controller) {
}

GoogleURLTrackerMapEntry::~GoogleURLTrackerMapEntry() {
}

void GoogleURLTrackerMapEntry::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(infobar_delegate_);
  DCHECK_EQ(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED, type);
  DCHECK_EQ(infobar_service_, content::Source<InfoBarService>(source).ptr());
  if (content::Details<InfoBar::RemovedDetails>(details)->first->delegate() ==
      infobar_delegate_) {
    google_url_tracker_->DeleteMapEntryForService(infobar_service_);
    // WARNING: At this point |this| has been deleted!
  }
}

void GoogleURLTrackerMapEntry::SetInfoBarDelegate(
    GoogleURLTrackerInfoBarDelegate* infobar_delegate) {
  DCHECK(!infobar_delegate_);
  infobar_delegate_ = infobar_delegate;
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                 content::Source<InfoBarService>(infobar_service_));
}

void GoogleURLTrackerMapEntry::Close(bool redo_search) {
  if (infobar_delegate_) {
    infobar_delegate_->Close(redo_search);
  } else {
    // WARNING: |infobar_service_| may point to a deleted object.  Do not
    // dereference it!  See GoogleURLTracker::OnTabClosed().
    google_url_tracker_->DeleteMapEntryForService(infobar_service_);
  }
  // WARNING: At this point |this| has been deleted!
}
