// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_API_INFOBARS_CONFIRM_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_API_INFOBARS_CONFIRM_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/api/infobars/infobar_delegate.h"

// An interface derived from InfoBarDelegate implemented by objects wishing to
// control a ConfirmInfoBar.
class ConfirmInfoBarDelegate : public InfoBarDelegate {
 public:
  enum InfoBarButton {
    BUTTON_NONE   = 0,
    BUTTON_OK     = 1 << 0,
    BUTTON_CANCEL = 1 << 1,
  };

  virtual ~ConfirmInfoBarDelegate();

  // Returns the InfoBar type to be displayed for the InfoBar.
  virtual InfoBarAutomationType GetInfoBarAutomationType() const OVERRIDE;

  // Returns the message string to be displayed for the InfoBar.
  virtual string16 GetMessageText() const = 0;

  // Return the buttons to be shown for this InfoBar.
  virtual int GetButtons() const;

  // Return the label for the specified button. The default implementation
  // returns "OK" for the OK button and "Cancel" for the Cancel button.
  virtual string16 GetButtonLabel(InfoBarButton button) const;

  // Return whether or not the specified button needs elevation.
  virtual bool NeedElevation(InfoBarButton button) const;

  // Called when the OK button is pressed. If this function returns true, the
  // infobar is then immediately closed. Subclasses MUST NOT return true if in
  // handling this call something triggers the infobar to begin closing.
  virtual bool Accept();

  // Called when the Cancel button is pressed. If this function returns true,
  // the infobar is then immediately closed. Subclasses MUST NOT return true if
  // in handling this call something triggers the infobar to begin closing.
  virtual bool Cancel();

  // Returns the text of the link to be displayed, if any. Otherwise returns
  // and empty string.
  virtual string16 GetLinkText() const;

  // Called when the Link (if any) is clicked. The |disposition| specifies how
  // the resulting document should be loaded (based on the event flags present
  // when the link was clicked). If this function returns true, the infobar is
  // then immediately closed. Subclasses MUST NOT return true if in handling
  // this call something triggers the infobar to begin closing.
  virtual bool LinkClicked(WindowOpenDisposition disposition);

 protected:
  explicit ConfirmInfoBarDelegate(InfoBarService* infobar_service);

  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;

 private:
  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar(InfoBarService* owner) OVERRIDE;
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const OVERRIDE;
  virtual ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBarDelegate);
};

#endif  // CHROME_BROWSER_API_INFOBARS_CONFIRM_INFOBAR_DELEGATE_H_
