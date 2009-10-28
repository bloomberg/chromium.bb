// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_INFOBAR_DELEGATE_H_

#include <string>

#include "chrome/browser/tab_contents/infobar_delegate.h"

class AutoFillManager;
class SkBitmap;
class TabContents;

// An InfoBar delegate that enables the user to allow or deny storing personal
// information gathered from a form submission.
class AutoFillInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  AutoFillInfoBarDelegate(TabContents* tab_contents, AutoFillManager* host);
  virtual ~AutoFillInfoBarDelegate();

  // ConfirmInfoBarDelegate implementation.
  virtual bool ShouldExpire(
      const NavigationController::LoadCommittedDetails& details) const;
  virtual void InfoBarClosed();
  virtual std::wstring GetMessageText() const;
  virtual SkBitmap* GetIcon() const;
  virtual int GetButtons() const;
  virtual std::wstring GetButtonLabel(
      ConfirmInfoBarDelegate::InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();

 private:
  // The autofill manager that initiated this infobar.
  AutoFillManager* host_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillInfoBarDelegate);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_INFOBAR_DELEGATE_H_
