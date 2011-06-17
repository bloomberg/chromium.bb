// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_BLOCKED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_TAB_CONTENTS_BLOCKED_INFOBAR_DELEGATE_H_

#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "googleurl/src/gurl.h"

class BlockedRunningInfoBarDelegate;

// Infobar class for blocked mixed content warnings.
class BlockedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:

  // Create an infobar with a localized message from the resource bundle
  // corresponding to |message_resource_id|, an allow button labeled with
  // the localized string corresponding to |button_resource_id|, and a link
  // labeled with a localized "learn more" string that takes you to |url|
  // when clicked.
  BlockedInfoBarDelegate(TabContentsWrapper* wrapper,
                         int message_resource_id,
                         int button_resource_id,
                         const GURL& url);

  // Type-checking downcast routine.
  virtual BlockedRunningInfoBarDelegate* AsBlockedRunningInfoBarDelegate();

 protected:
  TabContentsWrapper* wrapper() { return wrapper_; }

 private:
  // InfoBarDelegate:
  virtual BlockedInfoBarDelegate* AsBlockedInfoBarDelegate() OVERRIDE;

  // ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual string16 GetLinkText() OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  TabContentsWrapper* wrapper_;
  int message_resource_id_;
  int button_resource_id_;
  GURL url_;
};

// A subclass specific to the blocked displaying insecure content case.
class BlockedDisplayingInfoBarDelegate : public BlockedInfoBarDelegate {
 public:
  explicit BlockedDisplayingInfoBarDelegate(TabContentsWrapper* wrapper);
 private:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
};

// A subclass specific to the blocked running insecure content case.
class BlockedRunningInfoBarDelegate : public BlockedInfoBarDelegate {
 public:
  explicit BlockedRunningInfoBarDelegate(TabContentsWrapper* wrapper);
  virtual BlockedRunningInfoBarDelegate*
      AsBlockedRunningInfoBarDelegate() OVERRIDE;
 private:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_BLOCKED_INFOBAR_DELEGATE_H_

