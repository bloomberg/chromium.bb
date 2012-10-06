// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_INFOBAR_DELEGATE_H_

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "googleurl/src/gurl.h"

class GoogleURLTracker;
class InfoBarTabHelper;

// This infobar is shown by the GoogleURLTracker when the Google base URL has
// changed.
class GoogleURLTrackerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  GoogleURLTrackerInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                  GoogleURLTracker* google_url_tracker,
                                  const GURL& new_google_url);

  // ConfirmInfoBarDelegate:
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;

  // Allows GoogleURLTracker to change the Google base URL after the infobar has
  // been instantiated.  This should only be called with an URL with the same
  // TLD as the existing one, so that the prompt we're displaying will still be
  // correct.
  void SetGoogleURL(const GURL& new_google_url);

  bool showing() const { return showing_; }
  void set_pending_id(int pending_id) { pending_id_ = pending_id; }

  // These are virtual so test code can override them in a subclass.
  virtual void Show(const GURL& search_url);
  virtual void Close(bool redo_search);

 protected:
  virtual ~GoogleURLTrackerInfoBarDelegate();

  InfoBarTabHelper* map_key_;  // What |google_url_tracker_| uses to track us.
  GURL search_url_;
  GoogleURLTracker* google_url_tracker_;
  GURL new_google_url_;
  bool showing_;  // True if this delegate has been added to a TabContents.
  int pending_id_;

 private:
  // ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_INFOBAR_DELEGATE_H_
