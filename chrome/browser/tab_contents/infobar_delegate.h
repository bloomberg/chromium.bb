// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_TAB_CONTENTS_INFOBAR_DELEGATE_H_
#pragma once

#include "content/browser/tab_contents/infobar_delegate.h"

// An interface derived from InfoBarDelegate implemented by objects wishing to
// control a LinkInfoBar.
class LinkInfoBarDelegate : public InfoBarDelegate {
 public:
  // Returns the message string to be displayed in the InfoBar. |link_offset|
  // is the position where the link should be inserted.
  virtual string16 GetMessageTextWithOffset(size_t* link_offset) const = 0;

  // Returns the text of the link to be displayed.
  virtual string16 GetLinkText() const = 0;

  // Called when the Link is clicked. The |disposition| specifies how the
  // resulting document should be loaded (based on the event flags present when
  // the link was clicked). This function returns true if the InfoBar should be
  // closed now or false if it should remain until the user explicitly closes
  // it.
  virtual bool LinkClicked(WindowOpenDisposition disposition);

 protected:
  explicit LinkInfoBarDelegate(TabContents* contents);
  virtual ~LinkInfoBarDelegate();

 private:
  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar();
  virtual LinkInfoBarDelegate* AsLinkInfoBarDelegate();

  DISALLOW_COPY_AND_ASSIGN(LinkInfoBarDelegate);
};

// An interface derived from InfoBarDelegate implemented by objects wishing to
// control a ConfirmInfoBar.
class ConfirmInfoBarDelegate : public InfoBarDelegate {
 public:
  enum InfoBarButton {
    BUTTON_NONE   = 0,
    BUTTON_OK     = 1 << 0,
    BUTTON_CANCEL = 1 << 1,
  };

  // Returns the message string to be displayed for the InfoBar.
  virtual string16 GetMessageText() const = 0;

  // Return the buttons to be shown for this InfoBar.
  virtual int GetButtons() const;

  // Return the label for the specified button. The default implementation
  // returns "OK" for the OK button and "Cancel" for the Cancel button.
  virtual string16 GetButtonLabel(InfoBarButton button) const;

  // Return whether or not the specified button needs elevation.
  virtual bool NeedElevation(InfoBarButton button) const;

  // Called when the OK button is pressed. If the function returns true, the
  // InfoBarDelegate should be removed from the associated TabContents.
  virtual bool Accept();

  // Called when the Cancel button is pressed.  If the function returns true,
  // the InfoBarDelegate should be removed from the associated TabContents.
  virtual bool Cancel();

  // Returns the text of the link to be displayed, if any. Otherwise returns
  // and empty string.
  virtual string16 GetLinkText();

  // Called when the Link is clicked. The |disposition| specifies how the
  // resulting document should be loaded (based on the event flags present when
  // the link was clicked). This function returns true if the InfoBar should be
  // closed now or false if it should remain until the user explicitly closes
  // it.
  // Will only be called if GetLinkText() returns non-empty string.
  virtual bool LinkClicked(WindowOpenDisposition disposition);

 protected:
  explicit ConfirmInfoBarDelegate(TabContents* contents);
  virtual ~ConfirmInfoBarDelegate();

 private:
  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar();
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const;
  virtual ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate();

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBarDelegate);
};

// Simple implementations for common use cases ---------------------------------

class SimpleAlertInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SimpleAlertInfoBarDelegate(TabContents* contents,
                             SkBitmap* icon,  // May be NULL.
                             const string16& message,
                             bool auto_expire);

 private:
  virtual ~SimpleAlertInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual bool ShouldExpire(
      const NavigationController::LoadCommittedDetails& details) const;
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual string16 GetMessageText() const;
  virtual int GetButtons() const;

  SkBitmap* icon_;
  string16 message_;
  bool auto_expire_;  // Should it expire automatically on navigation?

  DISALLOW_COPY_AND_ASSIGN(SimpleAlertInfoBarDelegate);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_INFOBAR_DELEGATE_H_
