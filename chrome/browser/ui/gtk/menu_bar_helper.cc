// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/menu_bar_helper.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "ui/base/gtk/gtk_signal_registrar.h"

namespace {

// Recursively find all the GtkMenus that are attached to menu item |child|
// and add them to |data|, which is a vector of GtkWidgets.
void PopulateSubmenus(GtkWidget* child, gpointer data) {
  std::vector<GtkWidget*>* submenus =
      static_cast<std::vector<GtkWidget*>*>(data);
  GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(child));
  if (submenu) {
    submenus->push_back(submenu);
    gtk_container_foreach(GTK_CONTAINER(submenu), PopulateSubmenus, submenus);
  }
}

// Is the cursor over |menu| or one of its parent menus?
bool MotionIsOverMenu(GtkWidget* menu, GdkEventMotion* motion) {
  if (motion->x >= 0 && motion->y >= 0 &&
      motion->x < menu->allocation.width &&
      motion->y < menu->allocation.height) {
    return true;
  }

  while (menu) {
    GtkWidget* menu_item = gtk_menu_get_attach_widget(GTK_MENU(menu));
    if (!menu_item)
      return false;
    GtkWidget* parent = gtk_widget_get_parent(menu_item);

    if (gtk_util::WidgetContainsCursor(parent))
      return true;
    menu = parent;
  }

  return false;
}

}  // namespace

MenuBarHelper::MenuBarHelper(Delegate* delegate)
    : button_showing_menu_(NULL),
      showing_menu_(NULL),
      delegate_(delegate) {
  DCHECK(delegate_);
}

MenuBarHelper::~MenuBarHelper() {
}

void MenuBarHelper::Add(GtkWidget* button) {
  buttons_.push_back(button);
}

void MenuBarHelper::Remove(GtkWidget* button) {
  std::vector<GtkWidget*>::iterator iter =
      find(buttons_.begin(), buttons_.end(), button);
  if (iter == buttons_.end()) {
    NOTREACHED();
    return;
  }
  buttons_.erase(iter);
}

void MenuBarHelper::Clear() {
  buttons_.clear();
}

void MenuBarHelper::MenuStartedShowing(GtkWidget* button, GtkWidget* menu) {
  DCHECK(GTK_IS_MENU(menu));
  button_showing_menu_ = button;
  showing_menu_ = menu;

  signal_handlers_.reset(new ui::GtkSignalRegistrar());
  signal_handlers_->Connect(menu, "destroy",
                            G_CALLBACK(OnMenuHiddenOrDestroyedThunk), this);
  signal_handlers_->Connect(menu, "hide",
                            G_CALLBACK(OnMenuHiddenOrDestroyedThunk), this);
  signal_handlers_->Connect(menu, "motion-notify-event",
                            G_CALLBACK(OnMenuMotionNotifyThunk), this);
  signal_handlers_->Connect(menu, "move-current",
                            G_CALLBACK(OnMenuMoveCurrentThunk), this);
  gtk_container_foreach(GTK_CONTAINER(menu), PopulateSubmenus, &submenus_);

  for (size_t i = 0; i < submenus_.size(); ++i) {
    signal_handlers_->Connect(submenus_[i], "motion-notify-event",
                              G_CALLBACK(OnMenuMotionNotifyThunk), this);
  }
}

gboolean MenuBarHelper::OnMenuMotionNotify(GtkWidget* menu,
                                           GdkEventMotion* motion) {
  // Don't do anything if pointer is in the menu.
  if (MotionIsOverMenu(menu, motion))
    return FALSE;
  if (buttons_.empty())
    return FALSE;

  gint x = 0;
  gint y = 0;
  GtkWidget* last_button = NULL;

  for (size_t i = 0; i < buttons_.size(); ++i) {
    GtkWidget* button = buttons_[i];
    // Figure out coordinates relative to this button. Avoid using
    // gtk_widget_get_pointer() unnecessarily.
    if (i == 0) {
      // We have to make this call because the menu is a popup window, so it
      // doesn't share a toplevel with the buttons and we can't just use
      // gtk_widget_translate_coordinates().
      gtk_widget_get_pointer(buttons_[0], &x, &y);
    } else {
      gint last_x = x;
      gint last_y = y;
      if (!gtk_widget_translate_coordinates(
          last_button, button, last_x, last_y, &x, &y)) {
        // |button| may not be realized.
        continue;
      }
    }

    last_button = button;

    if (x >= 0 && y >= 0 && x < button->allocation.width &&
        y < button->allocation.height) {
      if (button != button_showing_menu_)
        delegate_->PopupForButton(button);
      return TRUE;
    }
  }

  return FALSE;
}

void MenuBarHelper::OnMenuHiddenOrDestroyed(GtkWidget* menu) {
  DCHECK_EQ(showing_menu_, menu);

  signal_handlers_.reset();
  showing_menu_ = NULL;
  button_showing_menu_ = NULL;
  submenus_.clear();
}

void MenuBarHelper::OnMenuMoveCurrent(GtkWidget* menu,
                                      GtkMenuDirectionType dir) {
  // The menu directions are triggered by the arrow keys as follows
  //
  //   PARENT   left
  //   CHILD    right
  //   NEXT     down
  //   PREV     up
  //
  // We only care about left and right. Note that for RTL, they are swapped.
  switch (dir) {
    case GTK_MENU_DIR_CHILD: {
      GtkWidget* active_item = GTK_MENU_SHELL(menu)->active_menu_item;
      // The move is going to open a submenu; don't override default behavior.
      if (active_item && gtk_menu_item_get_submenu(GTK_MENU_ITEM(active_item)))
        return;
      // Fall through.
    }
    case GTK_MENU_DIR_PARENT: {
      delegate_->PopupForButtonNextTo(button_showing_menu_, dir);
      break;
    }
    default:
      return;
  }

  // This signal doesn't have a return value; we have to manually stop its
  // propagation.
  g_signal_stop_emission_by_name(menu, "move-current");
}
