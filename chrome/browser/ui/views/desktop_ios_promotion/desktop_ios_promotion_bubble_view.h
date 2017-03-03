// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_util.h"
#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

class DesktopIOSPromotionController;
class Profile;

// The DesktopIOSPromotionBubbleView has the basic layout for the
// desktop to ios promotion view.
// This view will always be created as a subview of an existing
// bubble (ie. Password Bubble, Bookmark Bubble).
class DesktopIOSPromotionBubbleView : public DesktopIOSPromotionView,
                                      public views::View,
                                      public views::ButtonListener {
 public:
  DesktopIOSPromotionBubbleView(
      Profile* profile,
      desktop_ios_promotion::PromotionEntryPoint entry_point);
  ~DesktopIOSPromotionBubbleView() override;

  // DesktopIOSPromotionView:
  void UpdateRecoveryPhoneLabel() override;

 private:
  // ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::Button* send_sms_button_ = nullptr;
  views::Button* no_button_ = nullptr;
  // The text that will appear on the promotion body.
  views::Label* promotion_text_label_;
  std::unique_ptr<DesktopIOSPromotionController> promotion_controller_;

  DISALLOW_COPY_AND_ASSIGN(DesktopIOSPromotionBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_BUBBLE_VIEW_H_
