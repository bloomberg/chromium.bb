// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_AVATAR_BUTTON_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_AVATAR_BUTTON_MANAGER_H_

#include "chrome/browser/ui/views/profiles/avatar_button_style.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/features.h"

#if BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
namespace views {
class NavButtonProvider;
}
#endif

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

#if BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
  views::NavButtonProvider* get_nav_button_provider() {
    return nav_button_provider_;
  }
  void set_nav_button_provider(views::NavButtonProvider* nav_button_provider) {
    nav_button_provider_ = nav_button_provider;
  }
#endif

 private:
  BrowserNonClientFrameView* frame_view_;  // Weak. Owns |this|.

  // Menu button that displays the name of the active or guest profile.
  // May be null and will not be displayed for off the record profiles.
  views::View* view_;  // Owned by views hierarchy.

#if BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
  views::NavButtonProvider* nav_button_provider_ = nullptr;
#endif

  DISALLOW_COPY_AND_ASSIGN(AvatarButtonManager);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_AVATAR_BUTTON_MANAGER_H_
