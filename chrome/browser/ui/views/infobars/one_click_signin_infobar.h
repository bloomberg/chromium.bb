// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_ONE_CLICK_SIGNIN_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_ONE_CLICK_SIGNIN_INFOBAR_H_

#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

class OneClickSigninInfoBarDelegate;

// A speialization of ConfirmInfoBar that allows changing the background
// colour of the infobar as well as the colour of the OK button.  The behaviour
// remains the same.
class OneClickSigninInfoBar : public ConfirmInfoBar {
 public:
  OneClickSigninInfoBar(InfoBarService* owner,
                        OneClickSigninInfoBarDelegate* delegate);
  ~OneClickSigninInfoBar();

 private:
  // ConfirmInfoBar overrides.
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

  OneClickSigninInfoBarDelegate* one_click_delegate_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_ONE_CLICK_SIGNIN_INFOBAR_H_
