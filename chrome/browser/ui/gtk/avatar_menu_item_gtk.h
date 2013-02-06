// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_AVATAR_MENU_ITEM_GTK_H_
#define CHROME_BROWSER_UI_GTK_AVATAR_MENU_ITEM_GTK_H_

#include <gtk/gtk.h>

#include "chrome/browser/profiles/avatar_menu_model.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class GtkThemeService;

// This widget contains the profile icon, user name, and synchronization status
// to be displayed in the AvatarMenuBubble. Clicking the profile will open a new
// browser window, and when the user hovers over an active profile item, a link
// is displayed that will allow editing the profile.
class AvatarMenuItemGtk : public content::NotificationObserver {
 public:
  // Delegates opening or editing a profile.
  class Delegate {
   public:
    // Open a new browser window using the profile at |profile_index|.
    virtual void OpenProfile(size_t profile_index) = 0;

    // Edit the profile given by |profile_index|.
    virtual void EditProfile(size_t profile_index) = 0;
  };

  AvatarMenuItemGtk(Delegate* delegate,
                    const AvatarMenuModel::Item& item,
                    size_t item_index,
                    GtkThemeService* theme_service);
  virtual ~AvatarMenuItemGtk();

  // Returns the root widget for this menu item.
  GtkWidget* widget() { return widget_.get(); }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:

  void ShowStatusLabel();
  void ShowEditLink();

  CHROMEGTK_CALLBACK_1(AvatarMenuItemGtk, gboolean, OnProfileClick,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(AvatarMenuItemGtk, gboolean, OnProfileEnter,
                       GdkEventCrossing*);
  CHROMEGTK_CALLBACK_1(AvatarMenuItemGtk, gboolean, OnProfileLeave,
                       GdkEventCrossing*);
  CHROMEGTK_CALLBACK_1(AvatarMenuItemGtk, gboolean, OnProfileFocusIn,
                       GdkEventFocus*);
  CHROMEGTK_CALLBACK_1(AvatarMenuItemGtk, gboolean, OnProfileFocusOut,
                       GdkEventFocus*);
  CHROMEGTK_CALLBACK_1(AvatarMenuItemGtk, gboolean, OnProfileKeyPress,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_1(AvatarMenuItemGtk, gboolean, OnEventBoxExpose,
                       GdkEventExpose*);
  CHROMEGTK_CALLBACK_0(AvatarMenuItemGtk, void, OnEditProfileLinkClicked);

  // Create all widgets in this menu item, using |theme_service|.
  void Init(GtkThemeService* theme_service);

  // A weak pointer to the item's delegate.
  Delegate* delegate_;

  // Profile information to display for this item, e.g. user name, sync status.
  AvatarMenuModel::Item item_;

  // The index of this profile. The delegate uses this value to distinguish
  // which profile should be switched to.
  size_t item_index_;

  // The root widget for this menu item.
  ui::OwnedWidgetGtk widget_;

  // Provides colors.
  GtkThemeService* theme_service_;

  // A weak pointer to a label that displays the sync status. It is not shown
  // when the user is hovering over the item if the profile is the active
  // profile.
  GtkWidget* status_label_;

  // A weak pointer to a link button to edit the given profile. It is shown only
  // when the user is hovering over the active profile.
  GtkWidget* link_alignment_;

  // A weak pointer to a GtkChromeLinkButton so we can keep the use_gtk_theme
  // property up to date.
  GtkWidget* edit_profile_link_;

  // The highlighted color. Depending on the theme, this is either |widget|'s
  // bg[GTK_STATE_SELECTED] or a static highlight.
  GdkColor highlighted_color_;

  // The unhighlighted color. Depending on the theme, this is either NULL or a
  // pointer to static data.
  const GdkColor* unhighlighted_color_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuItemGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_AVATAR_MENU_ITEM_GTK_H_
