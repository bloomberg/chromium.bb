// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class replicates some menubar behaviors that are tricky to get right.
// It is used to create a more native feel for the bookmark bar.

#ifndef CHROME_BROWSER_UI_GTK_MENU_BAR_HELPER_H_
#define CHROME_BROWSER_UI_GTK_MENU_BAR_HELPER_H_
#pragma once

#include <gtk/gtk.h>

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "ui/base/gtk/gtk_signal.h"

namespace ui {
class GtkSignalRegistrar;
}

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
  virtual ~MenuBarHelper();

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
  CHROMEGTK_CALLBACK_0(MenuBarHelper, void, OnMenuHiddenOrDestroyed);
  CHROMEGTK_CALLBACK_1(MenuBarHelper, gboolean, OnMenuMotionNotify,
                       GdkEventMotion*);
  CHROMEGTK_CALLBACK_1(MenuBarHelper, void, OnMenuMoveCurrent,
                       GtkMenuDirectionType);

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

  // Signal handlers that are attached only between the "show" and "hide" events
  // for the menu.
  scoped_ptr<ui::GtkSignalRegistrar> signal_handlers_;

  Delegate* delegate_;
};

#endif  // CHROME_BROWSER_UI_GTK_MENU_BAR_HELPER_H_
