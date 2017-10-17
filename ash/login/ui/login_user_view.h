// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_USER_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_USER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/public/interfaces/login_user_info.mojom.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
}

namespace ash {

class HoverNotifier;
class LoginBubble;

// Display the user's profile icon, name, and a menu icon in various layout
// styles.
class ASH_EXPORT LoginUserView : public views::Button,
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

    bool is_opaque() const;

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
  void UpdateForUser(const mojom::LoginUserInfoPtr& user, bool animate);

  // Set if the view must be opaque.
  void SetForceOpaque(bool force_opaque);

  const mojom::LoginUserInfoPtr& current_user() const { return current_user_; }

  // views::Button:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void OnFocus() override;
  void OnBlur() override;

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

 private:
  // Called when hover state changes.
  void OnHover(bool has_hover);

  // Updates UI element values so they reflect the data in |current_user_|.
  void UpdateCurrentUserState();
  // Updates view opacity based on input state and |force_opaque_|.
  void UpdateOpacity();

  void SetLargeLayout();
  void SetSmallishLayout();

  class UserImage;
  class UserLabel;

  // Executed when the user view is pressed.
  OnTap on_tap_;

  // The user that is currently being displayed (or will be displayed when an
  // animation completes).
  mojom::LoginUserInfoPtr current_user_;

  // Used to dispatch opacity update events.
  std::unique_ptr<HoverNotifier> hover_notifier_;

  LoginDisplayStyle display_style_;
  UserImage* user_image_ = nullptr;
  UserLabel* user_label_ = nullptr;
  views::ImageButton* user_dropdown_ = nullptr;
  std::unique_ptr<LoginBubble> user_menu_;

  // True iff the view is currently opaque (ie, opacity = 1).
  bool is_opaque_ = false;
  // True if the view must be opaque (ie, opacity = 1) regardless of input
  // state.
  bool force_opaque_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoginUserView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_USER_VIEW_H_
