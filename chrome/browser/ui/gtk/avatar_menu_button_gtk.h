// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_AVATAR_MENU_BUTTON_GTK_H_
#define CHROME_BROWSER_UI_GTK_AVATAR_MENU_BUTTON_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class Browser;
class SkBitmap;

namespace gfx {
class Image;
}

// A button used to show the profile avatar. When clicked, it opens the
// AvatarMenuBubbleGtk.
class AvatarMenuButtonGtk {
 public:
  explicit AvatarMenuButtonGtk(Browser* browser);

  ~AvatarMenuButtonGtk();

  // Returns the button widget.
  GtkWidget* widget() const { return widget_.get(); }

  // Sets the location the arrow should be displayed on the menu bubble.
  void set_menu_arrow_location(BubbleGtk::ArrowLocationGtk arrow_location) {
    arrow_location_ = arrow_location;
  }

  // Sets the image to display on the button, typically the profile icon.
  void SetIcon(const gfx::Image& icon, bool is_gaia_picture);

  // Show the avatar bubble.
  void ShowAvatarBubble();

 private:
  CHROMEGTK_CALLBACK_1(AvatarMenuButtonGtk, gboolean, OnButtonPressed,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(AvatarMenuButtonGtk, void, OnSizeAllocate,
                       GtkAllocation*);

  void UpdateButtonIcon();

  // The button widget.
  ui::OwnedWidgetGtk widget_;

  // A weak pointer to the image widget displayed on the button.
  GtkWidget* image_;

  // A weak pointer to a browser. Used to create the bubble menu.
  Browser* browser_;

  // Which side of the bubble to display the arrow.
  BubbleGtk::ArrowLocationGtk arrow_location_;

  scoped_ptr<gfx::Image> icon_;
  bool is_gaia_picture_;
  int old_height_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuButtonGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_AVATAR_MENU_BUTTON_GTK_H_
