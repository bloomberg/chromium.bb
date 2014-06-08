// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/browser/google_url_tracker_map_entry.h"

#include "components/google/core/browser/google_url_tracker.h"
#include "components/infobars/core/infobar.h"

GoogleURLTrackerMapEntry::GoogleURLTrackerMapEntry(
    GoogleURLTracker* google_url_tracker,
    infobars::InfoBarManager* infobar_manager,
    scoped_ptr<GoogleURLTrackerNavigationHelper> navigation_helper)
    : google_url_tracker_(google_url_tracker),
      infobar_manager_(infobar_manager),
      infobar_delegate_(NULL),
      navigation_helper_(navigation_helper.Pass()),
      observing_(false) {
  DCHECK(infobar_manager_);
}

GoogleURLTrackerMapEntry::~GoogleURLTrackerMapEntry() {
  if (observing_)
    infobar_manager_->RemoveObserver(this);
}

void GoogleURLTrackerMapEntry::SetInfoBarDelegate(
    GoogleURLTrackerInfoBarDelegate* infobar_delegate) {
  DCHECK(!infobar_delegate_);

  // Transfer ownership of |navigation_helper_| to the infobar delegate as the
  // infobar delegate has need of it after this object has been destroyed in
  // the case where the user accepts the new Google URL.
  infobar_delegate->set_navigation_helper(navigation_helper_.Pass());
  infobar_delegate_ = infobar_delegate;
  infobar_manager_->AddObserver(this);
  observing_ = true;
}

void GoogleURLTrackerMapEntry::Close(bool redo_search) {
  if (infobar_delegate_) {
    infobar_delegate_->Close(redo_search);
  } else {
    // WARNING: |infobar_manager_| may point to a deleted object.  Do not
    // dereference it!  See GoogleURLTracker::OnTabClosed().
    google_url_tracker_->DeleteMapEntryForManager(infobar_manager_);
  }
  // WARNING: At this point |this| has been deleted!
}

void GoogleURLTrackerMapEntry::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                bool animate) {
  DCHECK(infobar_delegate_);
  if (infobar->delegate() == infobar_delegate_) {
    google_url_tracker_->DeleteMapEntryForManager(infobar_manager_);
    // WARNING: At this point |this| has been deleted!
  }
}

void GoogleURLTrackerMapEntry::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  DCHECK(observing_);
  DCHECK_EQ(infobar_manager_, manager);
  manager->RemoveObserver(this);
  observing_ = false;
}
