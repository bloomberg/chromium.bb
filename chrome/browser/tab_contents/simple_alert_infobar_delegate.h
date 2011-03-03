// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_TAB_CONTENTS_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"

class SkBitmap;
class TabContents;

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
      const NavigationController::LoadCommittedDetails& details) const OVERRIDE;
  virtual void InfoBarClosed() OVERRIDE;
  virtual SkBitmap* GetIcon() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;

  SkBitmap* icon_;
  string16 message_;
  bool auto_expire_;  // Should it expire automatically on navigation?

  DISALLOW_COPY_AND_ASSIGN(SimpleAlertInfoBarDelegate);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
