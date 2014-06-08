// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_MAP_ENTRY_H_
#define COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_MAP_ENTRY_H_

#include "base/memory/scoped_ptr.h"
#include "components/google/core/browser/google_url_tracker_infobar_delegate.h"
#include "components/google/core/browser/google_url_tracker_navigation_helper.h"
#include "components/infobars/core/infobar_manager.h"

class GoogleURLTracker;

namespace infobars {
class InfoBarManager;
}

class GoogleURLTrackerMapEntry : public infobars::InfoBarManager::Observer {
 public:
  GoogleURLTrackerMapEntry(
      GoogleURLTracker* google_url_tracker,
      infobars::InfoBarManager* infobar_manager,
      scoped_ptr<GoogleURLTrackerNavigationHelper> navigation_helper);
  virtual ~GoogleURLTrackerMapEntry();

  bool has_infobar_delegate() const { return !!infobar_delegate_; }
  GoogleURLTrackerInfoBarDelegate* infobar_delegate() {
    return infobar_delegate_;
  }
  void SetInfoBarDelegate(GoogleURLTrackerInfoBarDelegate* infobar_delegate);

  GoogleURLTrackerNavigationHelper* navigation_helper() {
    // This object gives ownership of |navigation_helper_| to the infobar
    // delegate in SetInfoBarDelegate().
    return has_infobar_delegate() ?
        infobar_delegate_->navigation_helper() : navigation_helper_.get();
  }

  const infobars::InfoBarManager* infobar_manager() const {
    return infobar_manager_;
  }

  void Close(bool redo_search);

 private:
  friend class GoogleURLTrackerTest;

  // infobars::InfoBarManager::Observer:
  virtual void OnInfoBarRemoved(infobars::InfoBar* infobar,
                                bool animate) OVERRIDE;
  virtual void OnManagerShuttingDown(
      infobars::InfoBarManager* manager) OVERRIDE;

  GoogleURLTracker* const google_url_tracker_;
  infobars::InfoBarManager* const infobar_manager_;
  GoogleURLTrackerInfoBarDelegate* infobar_delegate_;
  scoped_ptr<GoogleURLTrackerNavigationHelper> navigation_helper_;
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerMapEntry);
};

#endif  // COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_MAP_ENTRY_H_
