// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PARENT_ACCESS_VIEW_H_
#define ASH_LOGIN_UI_PARENT_ACCESS_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/login/ui/non_accessible_view.h"
#include "base/callback.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace ash {
class ArrowButtonView;
class LoginButton;
class LoginPinView;

// The view that allows for input of parent access code to authorize certain
// actions on child's device.
class ASH_EXPORT ParentAccessView : public NonAccessibleView,
                                    public views::ButtonListener {
 public:
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(ParentAccessView* view);
    ~TestApi();

    LoginButton* back_button() const;
    ArrowButtonView* submit_button() const;
    LoginPinView* pin_keyboard_view() const;

   private:
    ParentAccessView* const view_;
  };

  using OnSubmitCode = base::RepeatingCallback<void(const std::string& code)>;
  using OnBack = base::RepeatingClosure;

  // Parent access view callbacks.
  struct Callbacks {
    Callbacks();
    Callbacks(const Callbacks& other);
    ~Callbacks();

    // Called when user submits access code.
    OnSubmitCode on_submit;

    // Called when the user taps back button.
    OnBack on_back;
  };

  // Creates parent access view. |callbacks| will be called when user performs
  // certain actions.
  explicit ParentAccessView(const Callbacks& callbacks);
  ~ParentAccessView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void RequestFocus() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  class AccessCodeInput;

  // Called when completion state of |access_code_| changes. |complete| brings
  // information whether current input code is complete.
  void OnCodeComplete(bool complete);

  // Callbacks to be called when user performs certain actions.
  const Callbacks callbacks_;

  views::Label* title_label_ = nullptr;
  views::Label* description_label_ = nullptr;
  AccessCodeInput* access_code_view_ = nullptr;
  LoginPinView* pin_keyboard_view_ = nullptr;
  LoginButton* back_button_ = nullptr;
  views::LabelButton* help_button_ = nullptr;
  ArrowButtonView* submit_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ParentAccessView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PARENT_ACCESS_VIEW_H_
