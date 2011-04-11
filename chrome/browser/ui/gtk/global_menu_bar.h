// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GLOBAL_MENU_BAR_H_
#define CHROME_BROWSER_UI_GTK_GLOBAL_MENU_BAR_H_

#include <map>

#include "chrome/browser/command_updater.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

class Browser;
class BrowserWindowGtk;
struct GlobalMenuBarCommand;

typedef struct _GtkAccelGroup GtkAccelGroup;
typedef struct _GtkWidget GtkWidget;

// Controls the Mac style menu bar on Linux.
//
// Unity and some configurations of GNOME have a Apple-like menu bar at the top
// of the screen that changes depending on the active window. These mainly work
// by inspecting the application's widget hierarchy, and intercepting any
// GtkMenuBar found. Thankfully, these systems don't check to see if the menu
// bar itself is visible, so we insert a GtkMenuBar into the window hierarchy
// and set it to be invisible.
class GlobalMenuBar : public CommandUpdater::CommandObserver,
                      public NotificationObserver {
 public:
  GlobalMenuBar(Browser* browser, BrowserWindowGtk* window);
  virtual ~GlobalMenuBar();

  GtkWidget* widget() { return menu_bar_; }

 private:
  typedef std::map<int, GtkWidget*> IDMenuItemMap;

  // Helper function that builds the data.
  void BuildGtkMenuFrom(int menu_str_id,
                        std::map<int, GtkWidget*>* id_to_menu_item,
                        GlobalMenuBarCommand* commands);

  // CommandUpdater::CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  CHROMEGTK_CALLBACK_0(GlobalMenuBar, void, OnItemActivated);

  Browser* browser_;
  BrowserWindowGtk* browser_window_;

  NotificationRegistrar registrar_;

  // Our menu bar widget.
  GtkWidget* menu_bar_;

  // For some menu items, we want to show the accelerator, but not actually
  // explicitly handle it. To this end we connect those menu items' accelerators
  // to this group, but don't attach this group to any top level window.
  GtkAccelGroup* dummy_accel_group_;

  // A mapping from command ids to GtkMenuItem objects. We use this to update
  // the enable state since we are a .
  IDMenuItemMap id_to_menu_item_;

  // gtk_check_menu_item_set_active() will call the "activate" signal. We need
  // to block this activation while we change the checked state.
  bool block_activation_;
};

#endif  // CHROME_BROWSER_UI_GTK_GLOBAL_MENU_BAR_H_
