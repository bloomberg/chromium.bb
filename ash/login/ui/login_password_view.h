// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "ash/public/interfaces/login_user_info.mojom.h"
#include "ash/public/interfaces/user_info.mojom.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Button;
class ButtonListener;
class ImageButton;
class ImageView;
class Separator;
class Textfield;
}  // namespace views

namespace ash {

// Contains a textfield instance with a submit button. The user can type a
// password into the textfield and hit enter to submit.
//
// This view is always rendered via layers.
//
// The password view looks like this:
//
//   * * * * * *   =>
//  ------------------
class ASH_EXPORT LoginPasswordView
    : public views::View,
      public views::ButtonListener,
      public views::TextfieldController,
      public chromeos::input_method::ImeKeyboard::Observer {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginPasswordView* view);
    ~TestApi();

    views::View* textfield() const;
    views::View* submit_button() const;
    views::View* easy_unlock_icon() const;
    void set_immediately_hover_easy_unlock_icon();

   private:
    LoginPasswordView* view_;
  };

  using OnPasswordSubmit =
      base::RepeatingCallback<void(const base::string16& password)>;
  using OnPasswordTextChanged = base::RepeatingCallback<void(bool)>;
  using OnEasyUnlockIconHovered = base::RepeatingClosure;
  using OnEasyUnlockIconTapped = base::RepeatingClosure;

  // Must call |Init| after construction.
  LoginPasswordView();
  ~LoginPasswordView() override;

  // |on_submit| is called when the user hits enter or pressed the submit arrow.
  // |on_password_text_changed| is called when the text in the password field
  // changes.
  void Init(const OnPasswordSubmit& on_submit,
            const OnPasswordTextChanged& on_password_text_changed,
            const OnEasyUnlockIconHovered& on_easy_unlock_icon_hovered,
            const OnEasyUnlockIconTapped& on_easy_unlock_icon_tapped);

  // Change the active icon for easy unlock.
  void SetEasyUnlockIcon(mojom::EasyUnlockIconId id,
                         const base::string16& accessibility_label);

  // Updates accessibility information for |user|.
  void UpdateForUser(const mojom::LoginUserInfoPtr& user);

  // Enable or disable focus on the child elements (ie, password field and
  // submit button).
  void SetFocusEnabledForChildViews(bool enable);

  // Clear all currently entered text.
  void Clear();

  // Add the given numeric value to the textfield.
  void AppendNumber(int value);

  // Erase the last entered value.
  void Backspace();

  // Dispatch a submit event.
  void Submit();

  // Set password field placeholder. The password view cannot set the text by
  // itself because it doesn't know which auth methods are enabled.
  void SetPlaceholderText(const base::string16& placeholder_text);

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void RequestFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

  // chromeos::input_method::ImeKeyboard::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnLayoutChanging(const std::string& layout_name) override {}

 private:
  class EasyUnlockIcon;
  friend class TestApi;

  // Submits the current password field text to mojo call and resets the text
  // field.
  void SubmitPassword();

  OnPasswordSubmit on_submit_;
  OnPasswordTextChanged on_password_text_changed_;

  views::View* password_row_ = nullptr;

  views::Textfield* textfield_ = nullptr;
  views::ImageButton* submit_button_ = nullptr;
  views::ImageView* capslock_icon_ = nullptr;
  views::Separator* separator_ = nullptr;
  EasyUnlockIcon* easy_unlock_icon_ = nullptr;
  views::View* easy_unlock_right_margin_ = nullptr;

  ScopedObserver<chromeos::input_method::ImeKeyboard,
                 chromeos::input_method::ImeKeyboard::Observer>
      ime_keyboard_observer_;

  DISALLOW_COPY_AND_ASSIGN(LoginPasswordView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_
