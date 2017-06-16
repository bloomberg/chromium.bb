// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_

#include "ash/ash_export.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ButtonListener;
class Textfield;
}  // namespace views

namespace ash {

// Contains a textfield instance with a submit button. The user can type a
// password into the textfield and hit enter to submit.
//
// The password view looks like this:
//
//   * * * * * *   =>
//  ------------------
class ASH_EXPORT LoginPasswordView : public views::View,
                                     public views::ButtonListener {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginPasswordView* view);
    ~TestApi();

    views::View* textfield() const;
    views::View* submit_button() const;

   private:
    LoginPasswordView* view_;
  };

  using OnPasswordSubmit =
      base::RepeatingCallback<void(const base::string16& password)>;

  // |on_submit| is called when the user hits enter or pressed the submit arrow.
  // Must not be null.
  explicit LoginPasswordView(const OnPasswordSubmit& on_submit);
  ~LoginPasswordView() override;

  // Add the given numeric value to the textfield.
  void AppendNumber(int value);

  // Erase the last entered value.
  void Backspace();

  // Dispatch a submit event.
  void Submit();

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  friend class TestApi;

  // Submits the current password field text to mojo call and resets the text
  // field.
  void SubmitPassword();

  OnPasswordSubmit on_submit_;
  views::Textfield* textfield_;
  views::View* submit_button_;

  DISALLOW_COPY_AND_ASSIGN(LoginPasswordView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_
