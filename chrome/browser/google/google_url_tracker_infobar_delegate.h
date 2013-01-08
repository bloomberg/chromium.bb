// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_INFOBAR_DELEGATE_H_

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "googleurl/src/gurl.h"

class GoogleURLTracker;

// This infobar is shown by the GoogleURLTracker when the Google base URL has
// changed.
class GoogleURLTrackerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a Google URL tracker delegate and adds it to |infobar_service|.
  // Returns the delegate if it was successfully added.
  static GoogleURLTrackerInfoBarDelegate* Create(
      InfoBarService* infobar_service,
      GoogleURLTracker* google_url_tracker,
      const GURL& search_url);

  // ConfirmInfoBarDelegate:
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;

  // Other than set_pending_id(), these accessors are only used by test code.
  const GURL& search_url() const { return search_url_; }
  void set_search_url(const GURL& search_url) { search_url_ = search_url; }
  int pending_id() const { return pending_id_; }
  void set_pending_id(int pending_id) { pending_id_ = pending_id; }

  // These are virtual so test code can override them in a subclass.
  virtual void Update(const GURL& search_url);
  virtual void Close(bool redo_search);

 protected:
  GoogleURLTrackerInfoBarDelegate(InfoBarService* infobar_service,
                                  GoogleURLTracker* google_url_tracker,
                                  const GURL& search_url);
  virtual ~GoogleURLTrackerInfoBarDelegate();

 private:
  // ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;

  GoogleURLTracker* google_url_tracker_;
  GURL search_url_;
  int pending_id_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_INFOBAR_DELEGATE_H_
