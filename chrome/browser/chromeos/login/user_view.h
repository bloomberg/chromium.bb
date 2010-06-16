// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_VIEW_H_

#include "views/controls/button/button.h"
#include "views/view.h"

class SkBitmap;

namespace views {
class ImageView;
class Throbber;
}  // namespace views

namespace chromeos {

class SignoutView;

class UserView : public views::View {
 public:
  // Creates UserView for login screen.
  UserView();

  // Creates UserView for screen locker. This will have
  // a signout button in addition to the UserView used in the login screen.
  // |listener| is used to listen to button press event on this button.
  explicit UserView(views::ButtonListener* listener);

  virtual ~UserView() {}

  // view::View overrides.
  virtual gfx::Size GetPreferredSize();

  // Sets the user's image.
  void SetImage(const SkBitmap& image);

  // Start/Stop throbber.
  void StartThrobber();
  void StopThrobber();

  // Enable/Disable sign-out button.
  void SetSignoutEnabled(bool enabled);

 private:
  void Init();

  SignoutView* signout_view_;

  // For editing the password.
  views::ImageView* image_view_;

  views::Throbber* throbber_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_VIEW_H_

