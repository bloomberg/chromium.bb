// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_CONFIRM_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_CONFIRM_INFOBAR_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "components/infobars/core/infobar_delegate.h"

namespace infobars {
class InfoBar;
}

// An interface derived from InfoBarDelegate implemented by objects wishing to
// control a ConfirmInfoBar.
class ConfirmInfoBarDelegate : public infobars::InfoBarDelegate {
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
  virtual base::string16 GetMessageText() const = 0;

  // Returns the buttons to be shown for this InfoBar.
  virtual int GetButtons() const;

  // Returns the label for the specified button. The default implementation
  // returns "OK" for the OK button and "Cancel" for the Cancel button.
  virtual base::string16 GetButtonLabel(InfoBarButton button) const;

  // Returns whether or not the OK button will trigger a UAC elevation prompt on
  // Windows.
  virtual bool OKButtonTriggersUACPrompt() const;

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
  virtual base::string16 GetLinkText() const;

  // Called when the Link (if any) is clicked. The |disposition| specifies how
  // the resulting document should be loaded (based on the event flags present
  // when the link was clicked). If this function returns true, the infobar is
  // then immediately closed. Subclasses MUST NOT return true if in handling
  // this call something triggers the infobar to begin closing.
  virtual bool LinkClicked(WindowOpenDisposition disposition);

 protected:
  ConfirmInfoBarDelegate();

  // Returns a confirm infobar that owns |delegate|.
  static scoped_ptr<infobars::InfoBar> CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate> delegate);

  virtual bool ShouldExpireInternal(
      const NavigationDetails& details) const OVERRIDE;

 private:
  // InfoBarDelegate:
  virtual bool EqualsDelegate(
      infobars::InfoBarDelegate* delegate) const OVERRIDE;
  virtual ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBarDelegate);
};

#endif  // COMPONENTS_INFOBARS_CORE_CONFIRM_INFOBAR_DELEGATE_H_
