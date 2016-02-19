// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BUTTON_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BUTTON_DELEGATE_H_

#include "ui/views/controls/button/button.h"

// Delegate allowing NewAvatarButton to communicate back to its manager.
class AvatarButtonDelegate : public views::ButtonListener {
 public:
  // Called when the preferred size changed, e.g., due to a profile name change.
  virtual void ButtonPreferredSizeChanged() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BUTTON_DELEGATE_H_
