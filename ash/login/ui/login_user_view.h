// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_USER_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_USER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/public/interfaces/user_info.mojom.h"
#include "base/macros.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}

namespace ash {

// Display the user's profile icon, name, and a menu icon in various layout
// styles.
class ASH_EXPORT LoginUserView : public views::CustomButton,
                                 public views::ButtonListener {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginUserView* view);
    ~TestApi();

    LoginDisplayStyle display_style() const;

    const base::string16& displayed_name() const;

    views::View* user_label() const;

   private:
    LoginUserView* const view_;
  };

  using OnTap = base::RepeatingClosure;

  // Returns the width of this view for the given display style.
  static int WidthForLayoutStyle(LoginDisplayStyle style);

  LoginUserView(LoginDisplayStyle style,
                bool show_dropdown,
                const OnTap& on_tap);
  ~LoginUserView() override;

  // Update the user view to display the given user information.
  void UpdateForUser(const mojom::UserInfoPtr& user, bool animate);

  const mojom::UserInfoPtr& current_user() const { return current_user_; }

  // views::CustomButton:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

 private:
  // Updates UI element values so they reflect the data in |current_user_|.
  void UpdateCurrentUserState();

  void SetLargeLayout();
  void SetSmallishLayout();

  class UserImage;
  class UserLabel;

  // Executed when the user view is pressed.
  OnTap on_tap_;

  // The user that is currently being displayed (or will be displayed when an
  // animation completes).
  mojom::UserInfoPtr current_user_;

  LoginDisplayStyle display_style_;
  UserImage* user_image_ = nullptr;
  UserLabel* user_label_ = nullptr;
  views::ImageView* user_dropdown_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginUserView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_USER_VIEW_H_