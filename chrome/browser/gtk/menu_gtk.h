// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_MENU_GTK_H_
#define CHROME_BROWSER_GTK_MENU_GTK_H_

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "base/gfx/point.h"
#include "base/task.h"

class SkBitmap;

namespace menus {
class MenuModel;
}

struct MenuCreateMaterial;

class MenuGtk {
 public:
  // Delegate class that lets another class control the status of the menu.
  class Delegate {
   public:
    virtual ~Delegate() { }

    // Returns whether the menu item for this command should be enabled.
    virtual bool IsCommandEnabled(int command_id) const { return false; }

    // Returns whether this command is checked (for checkbox menu items only).
    virtual bool IsItemChecked(int command_id) const { return false; }

    // Gets the label. Only needs to be implemented for custom (dynamic) labels.
    virtual std::string GetLabel(int command_id) const { return std::string(); }

    // Executes the command.
    virtual void ExecuteCommand(int command_id) {}

    // Called when the menu stops showing. This will be called along with
    // ExecuteCommand if the user clicks an item, but will also be called when
    // the user clicks away from the menu.
    virtual void StoppedShowing() {}

    // Return true if we should override the "gtk-menu-images" system setting
    // when showing image menu items for this menu.
    virtual bool AlwaysShowImages() const { return false; }
  };

  // Builds a MenuGtk that uses |delegate| to perform actions and |menu_data|
  // to create the menu.
  MenuGtk(MenuGtk::Delegate* delegate, const MenuCreateMaterial* menu_data,
          GtkAccelGroup* accel_group);
  // Creates a MenuGtk that uses |delegate| to perform actions.  Builds the
  // menu using |model_| if non-NULL.
  // TODO(estade): MenuModel support is only partial. Only TYPE_SEPARATOR and
  // TYPE_COMMAND are currently implemented.
  MenuGtk(MenuGtk::Delegate* delegate, menus::MenuModel* model);
  ~MenuGtk();

  // Initialize GTK signal handlers.
  void ConnectSignalHandlers();

  // These methods are used to build the menu dynamically.
  void AppendMenuItemWithLabel(int command_id, const std::string& label);
  void AppendMenuItemWithIcon(int command_id, const std::string& label,
                              const SkBitmap& icon);
  void AppendCheckMenuItemWithLabel(int command_id, const std::string& label);
  void AppendSeparator();
  void AppendMenuItem(int command_id, GtkWidget* menu_item);

  // Displays the menu. |timestamp| is the time of activation. The popup is
  // statically positioned at |widget|.
  void Popup(GtkWidget* widget, gint button_type, guint32 timestamp);

  // Displays the menu using the button type and timestamp of |event|. The popup
  // is statically positioned at |widget|.
  void Popup(GtkWidget* widget, GdkEvent* event);

  // Displays the menu as a context menu, i.e. at the current cursor location.
  // |event_time| is the time of the event that triggered the menu's display.
  void PopupAsContext(guint32 event_time);

  // Displays the menu at the given coords. |point| is intentionally not const.
  void PopupAsContextAt(guint32 event_time, gfx::Point point);

  // Displays the menu following a keyboard event (such as selecting |widget|
  // and pressing "enter").
  void PopupAsFromKeyEvent(GtkWidget* widget);

  // Closes the menu.
  void Cancel();

  // Repositions the menu to be right under the button.  Alignment is set as
  // object data on |void_widget| with the tag "left_align".  If "left_align"
  // is true, it aligns the left side of the menu with the left side of the
  // button. Otherwise it aligns the right side of the menu with the right side
  // of the button. Public since some menus have odd requirements that don't
  // belong in a public class.
  static void WidgetMenuPositionFunc(GtkMenu* menu,
                                     int* x,
                                     int* y,
                                     gboolean* push_in,
                                     void* void_widget);

  // Positions the menu to appear at the gfx::Point represented by |userdata|.
  static void PointMenuPositionFunc(GtkMenu* menu,
                                    int* x,
                                    int* y,
                                    gboolean* push_in,
                                    gpointer userdata);

  GtkWidget* widget() const { return menu_; }

 private:
  // A recursive function that transforms a MenuCreateMaterial tree into a set
  // of GtkMenuItems.
  void BuildMenuIn(GtkWidget* menu,
                   const MenuCreateMaterial* menu_data,
                   GtkAccelGroup* accel_group);

  // Builds a GtkImageMenuItem.
  GtkWidget* BuildMenuItemWithImage(const std::string& label,
                                    const SkBitmap& icon);

  // A function that creates a GtkMenu from |model_|.
  void BuildMenuFromModel();

  // Contains implementation for OnMenuShow.
  void UpdateMenu();

  // Dispatches to either |model_| (if it is non-null) or |delegate_|. The
  // reason for this awkwardness is that we are in a transitional period where
  // we support both MenuModel and Delegate as a menu controller.
  // TODO(estade): remove controller functions from Delegate.
  // http://crbug.com/31365
  bool IsCommandEnabled(int id);
  void ExecuteCommand(int id);
  bool IsItemChecked(int id);

  // Callback for when a menu item is clicked.
  static void OnMenuItemActivated(GtkMenuItem* menuitem, MenuGtk* menu);

  // Sets the check mark and enabled/disabled state on our menu items.
  static void SetMenuItemInfo(GtkWidget* widget, void* raw_menu);

  // Updates all the menu items' state.
  static void OnMenuShow(GtkWidget* widget, MenuGtk* menu);

  // Sets the activating widget back to a normal appearance.
  static void OnMenuHidden(GtkWidget* widget, MenuGtk* menu);

  // Queries this object about the menu state.
  MenuGtk::Delegate* delegate_;

  // If non-NULL, the MenuModel that we use to populate and control the GTK
  // menu (overriding the delegate as a controller).
  menus::MenuModel* model_;

  // For some menu items, we want to show the accelerator, but not actually
  // explicitly handle it. To this end we connect those menu items' accelerators
  // to this group, but don't attach this group to any top level window.
  GtkAccelGroup* dummy_accel_group_;

  // gtk_menu_popup() does not appear to take ownership of popup menus, so
  // MenuGtk explicitly manages the lifetime of the menu.
  GtkWidget* menu_;

  // True when we should ignore "activate" signals.  Used to prevent
  // menu items from getting activated when we are setting up the
  // menu.
  static bool block_activation_;

  // We must free these at shutdown.
  std::vector<MenuGtk*> submenus_we_own_;

  ScopedRunnableMethodFactory<MenuGtk> factory_;
};

#endif  // CHROME_BROWSER_GTK_MENU_GTK_H_
