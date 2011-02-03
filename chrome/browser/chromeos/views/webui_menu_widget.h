// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIEWS_WEBUI_MENU_WIDGET_H_
#define CHROME_BROWSER_CHROMEOS_VIEWS_WEBUI_MENU_WIDGET_H_
#pragma once

#include <string>

#include "views/widget/widget_gtk.h"

class DOMView;
class ExtensionApiTest;

namespace chromeos {

class MenuLocator;
class NativeMenuDOMUI;

// WebUIMenuWidget is a window widget for a Web UI based menu.
class WebUIMenuWidget : public views::WidgetGtk {
 public:
  // Create a Window for the NativeMenuDMOUI. |root| specifies if
  // the menu is root menu.
  WebUIMenuWidget(NativeMenuDOMUI* domui_menu, bool root);
  virtual ~WebUIMenuWidget();

  // WidgetGtk overrides:
  virtual void Init(gfx::NativeView parent, const gfx::Rect& bounds);
  virtual void Hide();
  virtual void Close();
  virtual void ReleaseGrab();
  virtual gboolean OnGrabBrokeEvent(GtkWidget* widget, GdkEvent* event);
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);

  // Returns NativeMenuDOMUI that owns this widget.
  NativeMenuDOMUI* domui_menu() const {
    return domui_menu_;
  }

  // Returns true if the menu widget is a root.
  bool is_root() const {
    return is_root_;
  }

  // Returns true if the menu widget has input grab.
  bool did_input_grab() const {
    return did_input_grab_;
  }

  // Enables/Disables menu scroll.
  void EnableScroll(bool enabled);

  // Tell the gtk to send all input events (mouse, keyboard) to this
  // Widget. If |selectItem| is true, it highlights the selected item
  // (or select 1st selectable item if none is selected).
  void EnableInput(bool select_item);

  // Executes given |javascript|.
  void ExecuteJavascript(const std::wstring& javascript);

  // Show the menu using |locator|.  Ownership of locator is transferred
  // to this widget.
  void ShowAt(MenuLocator* locator);

  // Updates the size
  void SetSize(const gfx::Size& new_size);

  // Returns the menu locator owned by this widget.
  const MenuLocator* menu_locator() const {
    return menu_locator_.get();
  }

  // Returns WebUIMenuWidget that contains given native. This returns
  // NULL if not found.
  static WebUIMenuWidget* FindWebUIMenuWidget(gfx::NativeView native);

 private:
  // Capture the X pointer grab. This also enables input on the widget by
  // calling EnableInput(false).
  void CaptureGrab();

  // Clears GTK grab.
  void ClearGrabWidget();

  // NativeMenu object that owns this widget.
  NativeMenuDOMUI* domui_menu_;

  // DOMView to render the menu contents.
  DOMView* dom_view_;

  // MenuLocator that controls the position of this menu widget.
  scoped_ptr<chromeos::MenuLocator> menu_locator_;

  // True if the widget has input grab.
  bool did_input_grab_;

  // True if the widget is for root menu (very first menu in
  // submenu chain).
  bool is_root_;

  DISALLOW_COPY_AND_ASSIGN(WebUIMenuWidget);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VIEWS_WEBUI_MENU_WIDGET_H_
