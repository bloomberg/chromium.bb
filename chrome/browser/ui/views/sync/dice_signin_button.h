// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_DICE_SIGNIN_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_DICE_SIGNIN_BUTTON_H_

#include "base/macros.h"
#include "base/optional.h"
#include "components/signin/core/browser/account_info.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"

// Sign-in button used for Desktop Identity Consistency that presents the
// account information (avatar image and email) and allows the user to
// sign in to Chrome or to enable sync.
//
// The button also presents on the right hand side a drown-down arrow button
// that the user can interact with.
class DiceSigninButton : public views::MdTextButton {
 public:
  // Create a non-personalized sign-in button.
  // |button_listener| is called evey time the user interacts with this button.
  explicit DiceSigninButton(views::ButtonListener* button_listener);

  // Creates a sign-in button personalized with the data from |account|.
  // |button_listener| will be called for events originating from |this| or from
  // |drop_down_arrow|. The drop down arrow will only be shown when
  // |show_drop_down_arrow| is true.
  DiceSigninButton(const AccountInfo& account_info,
                   const gfx::Image& account_icon,
                   views::ButtonListener* button_listener,
                   bool show_drop_down_arrow = true);
  ~DiceSigninButton() override;

  const views::Button* drop_down_arrow() const { return arrow_; }
  const base::Optional<AccountInfo> account() const { return account_; }

  // views::MdTextButton:
  gfx::Rect GetChildAreaBounds() override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;

 private:
  const base::Optional<AccountInfo> account_;
  views::Label* subtitle_ = nullptr;
  views::View* divider_ = nullptr;
  views::ImageButton* arrow_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DiceSigninButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_DICE_SIGNIN_BUTTON_H_
