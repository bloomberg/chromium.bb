// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class DesktopIOSPromotionController;

// The DesktopIOSPromotionView has the basic layout for the
// desktop to ios promotion view.
// This view will always be created as a subview of an existing
// bubble (ie. Password Bubble, Bookmark Bubble).
class DesktopIOSPromotionView : public views::View,
                                public views::ButtonListener {
 public:
  explicit DesktopIOSPromotionView(desktop_ios_promotion::PromotionEntryPoint);

 private:
  // ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::Button* send_sms_button_ = nullptr;
  views::Button* no_button_ = nullptr;
  // TODO(crbug.com/676655): Add learn more link.

  DesktopIOSPromotionController* promotion_controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DesktopIOSPromotionView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_VIEW_H_
