// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_PIN_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_PIN_VIEW_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

// Implements a PIN keyboard. The class emits high-level events that can be used
// by the embedder. The PIN keyboard, while displaying letters, only emits
// numbers.
//
// The UI looks a little like this:
//    _______    _______    _______
//   |   1   |  |   2   |  |   3   |
//   |       |  | A B C |  | D E F |
//    -------    -------    -------
//    _______    _______    _______
//   |   1   |  |   2   |  |   3   |
//   | G H I |  | J K L |  | M N O |
//    -------    -------    -------
//    _______    _______    _______
//   |   1   |  |   2   |  |   3   |
//   |P Q R S|  | T U V |  |W X Y Z|
//    -------    -------    -------
//               _______    _______
//              |   0   |  |  <-   |
//              |   +   |  |       |
//               -------    -------
//
class ASH_EXPORT LoginPinView : public views::View {
 public:
  // Spacing between each pin button.
  static const int kButtonSeparatorSizeDp;
  // Size of each button.
  static const int kButtonSizeDp;

  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginPinView* view);
    ~TestApi();

    views::View* GetButton(int number) const;
    views::View* GetBackspaceButton() const;

   private:
    LoginPinView* const view_;
  };

  using OnPinKey = base::RepeatingCallback<void(int value)>;
  using OnPinBackspace = base::RepeatingClosure;

  // |on_key| is called whenever the user taps one of the pin buttons.
  // |on_backspace| is called when the user wants to erase the most recently
  // tapped key. Neither callback can be null.
  explicit LoginPinView(const OnPinKey& on_key,
                        const OnPinBackspace& on_backspace);
  ~LoginPinView() override;

  // views::View:
  const char* GetClassName() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

 private:
  OnPinKey on_key_;
  OnPinBackspace on_backspace_;

  DISALLOW_COPY_AND_ASSIGN(LoginPinView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_PIN_VIEW_H_
