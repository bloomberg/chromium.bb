// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_H_

#include "base/mac/scoped_nsobject.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_controller.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_delegate.h"

// InfoBar for iOS acts as a UIViewController for InfoBarView.
class InfoBarIOS : public infobars::InfoBar, public InfoBarViewDelegate {
 public:
  explicit InfoBarIOS(scoped_ptr<infobars::InfoBarDelegate> delegate);
  ~InfoBarIOS() override;

  // Layouts the infobar using data from delegate and prepare it for adding to
  // superview.
  void Layout(CGRect container_bounds);

  // Returns UIView holding content of this infobar.
  UIView* view();

  // Remove the infobar view from infobar container view.
  void RemoveView();

  // Sets the controller. Should be called right after the infobar's creation.
  void SetController(InfoBarController* controller);

 private:
  // InfoBar overrides:
  void PlatformSpecificOnHeightsRecalculated() override;

  // InfoBarViewDelegate:
  void SetInfoBarTargetHeight(int height) override;
  void InfoBarDidCancel() override;
  void InfoBarButtonDidPress(NSUInteger button_id) override;

  base::scoped_nsobject<InfoBarController> controller_;
  DISALLOW_COPY_AND_ASSIGN(InfoBarIOS);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_H_
