// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLOBAL_MENU_BAR_X11_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLOBAL_MENU_BAR_X11_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/command_observer.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/views/widget/desktop_aura/desktop_root_window_host_observer_x11.h"

typedef struct _DbusmenuMenuitem DbusmenuMenuitem;
typedef struct _DbusmenuServer   DbusmenuServer;

namespace ui {
class Accelerator;
}

class Browser;
class BrowserView;

class BrowserDesktopRootWindowHostX11;
struct GlobalMenuBarCommand;

// Controls the Mac style menu bar on Unity.
//
// Unity has an Apple-like menu bar at the top of the screen that changes
// depending on the active window. In the GTK port, we had a hidden GtkMenuBar
// object in each GtkWindow which existed only to be scrapped by the
// libdbusmenu-gtk code. Since we don't have GtkWindows anymore, we need to
// interface directly with the lower level libdbusmenu-glib, which we
// opportunistically dlopen() since not everyone is running Ubuntu.
class GlobalMenuBarX11 : public CommandObserver,
                         public views::DesktopRootWindowHostObserverX11 {
 public:
  GlobalMenuBarX11(BrowserView* browser_view,
                   BrowserDesktopRootWindowHostX11* host);
  virtual ~GlobalMenuBarX11();

  // Creates the object path for DbusemenuServer which is attached to |xid|.
  static std::string GetPathForWindow(unsigned long xid);

 private:
  typedef std::map<int, DbusmenuMenuitem*> CommandIDMenuItemMap;

  // Creates a DbusmenuServer, and attaches all the menu items.
  void InitServer(unsigned long xid);

  // Stops listening to enable state changed events.
  void Disable();

  // Creates a whole menu defined with |commands| and titled with the string
  // |menu_str_id|. Then appends it to |parent|.
  void BuildMenuFrom(DbusmenuMenuitem* parent,
                     int menu_str_id,
                     CommandIDMenuItemMap* id_to_menu_item,
                     GlobalMenuBarCommand* commands);

  // Creates an individual menu item from a title and command, and subscribes
  // to the activation signal.
  DbusmenuMenuitem* BuildMenuItem(
      int string_id,
      int command_id,
      int tag_id,
      CommandIDMenuItemMap* id_to_menu_item);

  // Sets the accelerator for |item|.
  void RegisterAccelerator(DbusmenuMenuitem* item,
                           const ui::Accelerator& accelerator);

  // Updates the visibility of the bookmark bar on pref changes.
  void OnBookmarkBarVisibilityChanged();

  // Overridden from CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled) OVERRIDE;

  // Overridden from DesktopRootWindowHostObserverX11:
  virtual void OnWindowMapped(unsigned long xid) OVERRIDE;
  virtual void OnWindowUnmapped(unsigned long xid) OVERRIDE;

  CHROMEG_CALLBACK_1(GlobalMenuBarX11, void, OnItemActivated, DbusmenuMenuitem*,
                     unsigned int);

  Browser* browser_;
  BrowserView* browser_view_;
  BrowserDesktopRootWindowHostX11* host_;

  // Maps command ids to DbusmenuMenuitems so we can modify their
  // enabled/checked state in response to state change notifications.
  CommandIDMenuItemMap id_to_menu_item_;

  DbusmenuServer* server_;
  DbusmenuMenuitem* root_item_;

  // Tracks value of the kShowBookmarkBar preference.
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(GlobalMenuBarX11);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLOBAL_MENU_BAR_X11_H_
