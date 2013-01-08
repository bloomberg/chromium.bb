// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_INFOBAR_DELEGATE_H_

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "third_party/skia/include/core/SkColor.h"

// An interface derived from ConfirmInfoBarDelegate implemented for the
// one-click sign in inforbar experiments.
class OneClickSigninInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // A set of alternate colours for the experiment.  The colour values are only
  // used if |enabled| is true.  Otherwise the infobar uses its default colours.
  struct AlternateColors {
    bool enabled;
    SkColor infobar_top_color;
    SkColor infobar_bottom_color;
    SkColor button_text_color;
    SkColor button_background_color;
    SkColor button_border_color;
  };

  virtual ~OneClickSigninInfoBarDelegate();

  // Returns the colours to use with the one click signin infobar.  The colours
  // depend on the experimental group.
  virtual void GetAlternateColors(AlternateColors* alt_colors);

 protected:
  explicit OneClickSigninInfoBarDelegate(InfoBarService* infobar_service);

 private:
#if defined(TOOLKIT_VIEWS)
   // Because the experiment is only running for views, only override this
   // function there.  Other platforms that support one-click sign in will
   // create a regular confirm infobar with no support for colour changes.

  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar(InfoBarService* owner) OVERRIDE;
#endif

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_INFOBAR_DELEGATE_H_
