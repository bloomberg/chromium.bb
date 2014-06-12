// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_LABEL_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_LABEL_H_

#include "base/compiler_specific.h"
#include "ui/views/controls/button/label_button.h"

class BrowserView;

// AvatarLabel
//
// A label used to display a string indicating that the current profile belongs
// to a supervised user.
class AvatarLabel : public views::LabelButton {
 public:
  explicit AvatarLabel(BrowserView* browser_view);
  virtual ~AvatarLabel();

  // views::LabelButton:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;

  // Update the style of the label according to the provided theme.
  void UpdateLabelStyle();

  // Sets whether the label should be displayed on the right or on the left. A
  // new button border is created which has the right insets for the positioning
  // of the button.
  void SetLabelOnRight(bool label_on_right);

 private:
  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(AvatarLabel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_LABEL_H_
