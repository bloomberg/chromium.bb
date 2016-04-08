// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_MENU_BUTTON_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view_targeter_delegate.h"

namespace gfx {
class Canvas;
class Image;
}
class BrowserNonClientFrameView;
class BrowserView;
class Profile;

// AvatarMenuButton
//
// A button used to show either the incognito avatar or the profile avatar.
// The button can optionally have a menu attached to it.

class AvatarMenuButton : public views::MenuButton,
                         public views::MenuButtonListener,
                         public views::ViewTargeterDelegate {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // Creates a new button for the given browser view.
  explicit AvatarMenuButton(BrowserView* browser_view);

  ~AvatarMenuButton() override;

  // views::MenuButton:
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // Sets the image for the avatar button. Rectangular images, as opposed
  // to Chrome avatar icons, will be resized and modified for the title bar.
  virtual void SetAvatarIcon(const gfx::Image& icon, bool is_rectangle);

  // Get avatar images. |avatar| is used in the browser window whereas
  // |taskbar_badge_avatar| is used for the OS taskbar. If
  // |taskbar_badge_avatar| is empty then |avatar| should be used for the
  // taskbar as well. Returns false if the cache doesn't have an entry for a
  // Profile::REGULAR_PROFILE type Profile, otherwise return true.
  static bool GetAvatarImages(const BrowserNonClientFrameView* frame_view,
                              bool should_show_avatar_menu,
                              gfx::Image* avatar,
                              gfx::Image* taskbar_badge_avatar,
                              bool* is_rectangle);

 private:
  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // views::MenuButtonListener:
  void OnMenuButtonClicked(views::MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override;

  BrowserView* browser_view_;
  bool enabled_;
  std::unique_ptr<ui::MenuModel> menu_model_;

  // Use a scoped ptr because gfx::Image doesn't have a default constructor.
  std::unique_ptr<gfx::Image> icon_;
  gfx::ImageSkia button_icon_;
  bool is_rectangle_;
  int old_height_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_MENU_BUTTON_H_
