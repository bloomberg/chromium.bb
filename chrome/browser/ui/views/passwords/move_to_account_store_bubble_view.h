// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MOVE_TO_ACCOUNT_STORE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MOVE_TO_ACCOUNT_STORE_BUBBLE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

// Bubble asking the user to move a profile credential to their account store.
class MoveToAccountStoreBubbleView : public PasswordBubbleViewBase {
 public:
  explicit MoveToAccountStoreBubbleView(content::WebContents* web_contents,
                                        views::View* anchor_view);
  ~MoveToAccountStoreBubbleView() override;

 private:
  // PasswordBubbleViewBase
  void AddedToWidget() override;
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;
  bool ShouldShowCloseButton() const override;
  MoveToAccountStoreBubbleController* GetController() override;
  const MoveToAccountStoreBubbleController* GetController() const override;

  MoveToAccountStoreBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MOVE_TO_ACCOUNT_STORE_BUBBLE_VIEW_H_
