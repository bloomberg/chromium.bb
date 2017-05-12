// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_AVATAR_BUTTON_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_AVATAR_BUTTON_MANAGER_H_

#include "chrome/browser/ui/views/profiles/avatar_button_style.h"
#include "ui/views/controls/button/button.h"

class BrowserNonClientFrameView;

// Manages an avatar button displayed in a browser frame. The button displays
// the name of the active or guest profile, and may be null.
class AvatarButtonManager : public views::ButtonListener {
 public:
  explicit AvatarButtonManager(BrowserNonClientFrameView* frame_view);

  // Adds or removes the avatar button from the frame, based on the BrowserView
  // properties.
  void Update(AvatarButtonStyle style);

  // Gets the avatar button as a view::View.
  views::View* view() const { return view_; }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  BrowserNonClientFrameView* frame_view_;  // Weak. Owns |this|.

  // Menu button that displays the name of the active or guest profile.
  // May be null and will not be displayed for off the record profiles.
  views::View* view_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AvatarButtonManager);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_AVATAR_BUTTON_MANAGER_H_
