// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_INFOBAR_DELEGATE_H_
#define COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

class GoogleURLTracker;
class GoogleURLTrackerNavigationHelper;

namespace infobars {
class InfoBarManager;
}

// This infobar is shown by the GoogleURLTracker when the Google base URL has
// changed.
class GoogleURLTrackerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a Google URL tracker infobar and delegate and adds the infobar to
  // |infobar_manager|.  Returns the infobar if it was successfully added.
  static infobars::InfoBar* Create(
      infobars::InfoBarManager* infobar_manager,
      GoogleURLTracker* google_url_tracker,
      const GURL& search_url);

  // ConfirmInfoBarDelegate:
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  GoogleURLTrackerNavigationHelper* navigation_helper() {
    return navigation_helper_weak_ptr_;
  }

  void set_navigation_helper(
      scoped_ptr<GoogleURLTrackerNavigationHelper> navigation_helper) {
    navigation_helper_ = navigation_helper.Pass();
    navigation_helper_weak_ptr_ = navigation_helper_.get();
  }

  // Other than set_pending_id(), these accessors are only used by test code.
  const GURL& search_url() const { return search_url_; }
  void set_search_url(const GURL& search_url) { search_url_ = search_url; }
  int pending_id() const { return pending_id_; }
  void set_pending_id(int pending_id) { pending_id_ = pending_id; }

  // These are virtual so test code can override them in a subclass.
  virtual void Update(const GURL& search_url);
  virtual void Close(bool redo_search);

 protected:
  GoogleURLTrackerInfoBarDelegate(
      GoogleURLTracker* google_url_tracker,
      const GURL& search_url);
  virtual ~GoogleURLTrackerInfoBarDelegate();

 private:
  // ConfirmInfoBarDelegate:
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual base::string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;
  virtual bool ShouldExpireInternal(
      const NavigationDetails& details) const OVERRIDE;

  GoogleURLTracker* google_url_tracker_;
  scoped_ptr<GoogleURLTrackerNavigationHelper> navigation_helper_;

  // During Close(), this object gives up ownership of |navigation_helper_|,
  // which then outlives this object. Sometimes after this point, other classes
  // still attempt to call navigation_helper() to access the (still-valid)
  // instance. The NavigationHelper instance is stored as a weak pointer in
  // addition to a strong pointer to facilitate this case.
  GoogleURLTrackerNavigationHelper* navigation_helper_weak_ptr_;
  GURL search_url_;
  int pending_id_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerInfoBarDelegate);
};

#endif  // COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_INFOBAR_DELEGATE_H_
