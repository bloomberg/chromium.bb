// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLLECTED_COOKIES_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_COLLECTED_COOKIES_INFOBAR_DELEGATE_H_

#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"

// This class configures an infobar shown when the collected cookies dialog
// is closed and the settings for one or more cookies have been changed.  The
// user is shown a message indicating that a reload of the page is
// required for the changes to take effect, and presented a button to perform
// the reload right from the infobar.
class CollectedCookiesInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit CollectedCookiesInfoBarDelegate(InfoBarTabHelper* infobar_helper);

 private:
  // ConfirmInfoBarDelegate overrides.
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_COLLECTED_COOKIES_INFOBAR_DELEGATE_H_
