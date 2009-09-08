// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class replicates some menubar behaviors that are tricky to get right.
// It is used to create a more native feel for the bookmark bar and the
// page/app menus.

#ifndef CHROME_BROWSER_GTK_MENU_BAR_HELPER_H_
#define CHROME_BROWSER_GTK_MENU_BAR_HELPER_H_

#include <gtk/gtk.h>

#include <vector>

class MenuBarHelper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when a the menu for a button ought to be triggered.
    virtual void PopupForButton(GtkWidget* button) = 0;
    virtual void PopupForButtonNextTo(GtkWidget* button,
                                      GtkMenuDirectionType dir) = 0;
  };

  // |delegate| cannot be null.
  explicit MenuBarHelper(Delegate* delegate);
  ~MenuBarHelper();

  // Must be called whenever a button's menu starts showing. It triggers the
  // MenuBarHelper to start listening for certain events.
  void MenuStartedShowing(GtkWidget* button, GtkWidget* menu);

  // Add |button| to the set of buttons we care about.
  void Add(GtkWidget* button);

  // Remove |button| from the set of buttons we care about.
  void Remove(GtkWidget* button);

  // Clear all buttons from the set.
  void Clear();

 private:
  static gboolean OnMenuMotionNotifyThunk(GtkWidget* menu,
                                          GdkEventMotion* motion,
                                          MenuBarHelper* helper) {
    return helper->OnMenuMotionNotify(menu, motion);
  }
  gboolean OnMenuMotionNotify(GtkWidget* menu, GdkEventMotion* motion);

  static void OnMenuHiddenThunk(GtkWidget* menu, MenuBarHelper* helper) {
    helper->OnMenuHidden(menu);
  }
  void OnMenuHidden(GtkWidget* menu);

  static void OnMenuMoveCurrentThunk(GtkWidget* menu,
                                     GtkMenuDirectionType dir,
                                     MenuBarHelper* helper) {
    helper->OnMenuMoveCurrent(menu, dir);
  }
  void OnMenuMoveCurrent(GtkWidget* menu, GtkMenuDirectionType dir);

  // The buttons for which we pop up menus. We do not own these, or even add
  // refs to them.
  std::vector<GtkWidget*> buttons_;

  // The button that is currently showing a menu, or NULL.
  GtkWidget* button_showing_menu_;

  // The highest level menu that is currently showing, or NULL.
  GtkWidget* showing_menu_;

  // All the submenus of |showing_menu_|. We connect to motion events on all
  // of them.
  std::vector<GtkWidget*> submenus_;

  Delegate* delegate_;
};

#endif  // CHROME_BROWSER_GTK_MENU_BAR_HELPER_H_
