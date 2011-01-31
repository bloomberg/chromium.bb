// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_VIEW_H_
#pragma once

#include <string>

#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/view.h"

class SkBitmap;

namespace views {
class ImageView;
class TextButton;
class Throbber;
}  // namespace views

namespace chromeos {

class SignoutView;
class PodImageView;

class UserView : public views::View,
                 public views::LinkController,
                 public views::ButtonListener {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Notifies that user pressed signout button on screen locker.
    virtual void OnSignout() {}

    // Notifies that user would like to remove this user from login screen.
    virtual void OnRemoveUser() {}

    // Returns true if current user is selected.
    virtual bool IsUserSelected() const = 0;
  };

  // Creates UserView for login screen (|is_login| == true) or screen locker.
  // On login screen this will have remove button.
  // On screen locker it will have sign out button. |need_background| is needed
  // to show image with transparent areas.
  UserView(Delegate* delegate, bool is_login, bool need_background);

  // view::View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void OnLocaleChanged();

  // Sets the user's image. If image's size is less than
  // 75% of window size, image size is preserved to avoid blur. Otherwise,
  // the image is resized to fit window size precisely. Image view repaints
  // itself.
  void SetImage(const SkBitmap& image, const SkBitmap& image_hot);

  // Sets tooltip over the image.
  void SetTooltipText(const std::wstring& text);

  // Show/Hide remove button.
  void SetRemoveButtonVisible(bool flag);

  // Enable/Disable sign-out button.
  void SetSignoutEnabled(bool enabled);

  // Implements LinkController.
  // Called when a signout link is clicked.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 private:
  void Init(bool need_background);

  Delegate* delegate_;

  SignoutView* signout_view_;
  PodImageView* image_view_;

  views::TextButton* remove_button_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_VIEW_H_
