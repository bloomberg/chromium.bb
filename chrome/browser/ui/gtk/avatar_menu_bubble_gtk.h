// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_AVATAR_MENU_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_AVATAR_MENU_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/ui/gtk/avatar_menu_item_gtk.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

class AvatarMenuModel;
class Browser;
class GtkThemeService;

// This bubble is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class AvatarMenuBubbleGtk : public BubbleDelegateGtk,
                            public AvatarMenuModelObserver,
                            public AvatarMenuItemGtk::Delegate {
 public:
  AvatarMenuBubbleGtk(Browser* browser,
                      GtkWidget* anchor,
                      BubbleGtk::FrameStyle arrow,
                      const gfx::Rect* rect);
  virtual ~AvatarMenuBubbleGtk();

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble,
                             bool closed_by_escape) OVERRIDE;

  // AvatarMenuModelObserver implementation.
  virtual void OnAvatarMenuModelChanged(
      AvatarMenuModel* avatar_menu_model) OVERRIDE;

  // AvatarMenuItemGtk::Delegate implementation.
  virtual void OpenProfile(size_t profile_index) OVERRIDE;
  virtual void EditProfile(size_t profile_index) OVERRIDE;

 private:
  // Notified when |contents_| is destroyed so we can delete our instance.
  CHROMEGTK_CALLBACK_0(AvatarMenuBubbleGtk, void, OnDestroy);
  CHROMEGTK_CALLBACK_1(AvatarMenuBubbleGtk, void, OnSizeRequest,
                       GtkRequisition*);
  CHROMEGTK_CALLBACK_0(AvatarMenuBubbleGtk, void, OnNewProfileLinkClicked);

  // Create all widgets in this bubble.
  void InitContents();

  // Close the bubble and set bubble_ to NULL.
  void CloseBubble();

  // A model of all the profile information to be displayed in the menu.
  scoped_ptr<AvatarMenuModel> avatar_menu_model_;

  // A weak pointer to the parent widget of all widgets in the bubble.
  GtkWidget* contents_;

  // A weak pointer to the bubble window.
  BubbleGtk* bubble_;

  // A weak pointer to the theme service.
  GtkThemeService* theme_service_;

  // A weak pointer to the new proifle link to keep its theme information
  // updated.
  GtkWidget* new_profile_link_;

  // A vector of all profile items in the menu.
  ScopedVector<AvatarMenuItemGtk> items_;

  // The minimum width to display the bubble. This is used to prevent the bubble
  // from automatically reducing its size when hovering over a profile item.
  int minimum_width_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_AVATAR_MENU_BUBBLE_GTK_H_
