// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "views/context_menu_controller.h"
#include "views/controls/menu/menu_listener.h"

class StatusAreaButton;

namespace views {
class AccessiblePaneView;
class MenuDelegate;
class MenuRunner;
}  // namespace views

namespace chromeos {

class LayoutModeButton;
class StatusAreaViewChromeos;

// chromeos::BrowserView adds ChromeOS specific controls and menus to a
// BrowserView created with Browser::TYPE_TABBED. This extender adds controls
// to the title bar as follows:
//       ____  __ __
//      /    \   \  \     [StatusArea] [LayoutModeButton]
//
// and adds the system context menu to the remaining area of the titlebar.
class BrowserView : public ::BrowserView,
                    public views::ContextMenuController,
                    public views::MenuListener,
                    public BrowserList::Observer,
                    public StatusAreaButton::Delegate,
                    public MessageLoopForUI::Observer {
 public:
  explicit BrowserView(Browser* browser);
  virtual ~BrowserView();

  // Adds a new tray icon/button to the Chrome OS Tray.
  // Takes ownership of the button object.
  void AddTrayButton(StatusAreaButton* button, bool bordered);

  // Remove an existing tray button from the Chrome OS Tray.
  // Pointer will become invalid after this call.
  void RemoveTrayButton(StatusAreaButton* button);

  // Check if a button is currently contained in the view.
  bool ContainsButton(StatusAreaButton* button);

  static BrowserView* GetBrowserViewForBrowser(Browser* browser);

  // BrowserView implementation.
  virtual void Init() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void FocusChromeOSStatus() OVERRIDE;
  virtual views::LayoutManager* CreateLayoutManager() const OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual bool GetSavedWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;
  virtual void Cut() OVERRIDE;
  virtual void Copy() OVERRIDE;
  virtual void Paste() OVERRIDE;
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) OVERRIDE;

  // views::ContextMenuController implementation.
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& p,
                                      bool is_mouse_gesture) OVERRIDE;

  // views::MenuListener implementation.
  virtual void OnMenuOpened() OVERRIDE;

  // BrowserList::Observer implementation.
  virtual void OnBrowserAdded(const Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(const Browser* browser) OVERRIDE;

  // StatusAreaButton::Delegate overrides.
  virtual bool ShouldExecuteStatusAreaCommand(
      const views::View* button_view, int command_id) const OVERRIDE;
  virtual void ExecuteStatusAreaCommand(
      const views::View* button_view, int command_id) OVERRIDE;
  virtual gfx::Font GetStatusAreaFont(const gfx::Font& font) const OVERRIDE;
  virtual StatusAreaButton::TextStyle GetStatusAreaTextStyle() const OVERRIDE;
  virtual void ButtonVisibilityChanged(views::View* button_view) OVERRIDE;

  // MessageLoopForUI::Observer overrides.
#if defined(USE_AURA)
  // MessageLoopForUI::Observer overrides.
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE;
#else
  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE {}
  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE;
#endif

  gfx::NativeView saved_focused_widget() const {
    return saved_focused_widget_;
  }

  bool has_hide_status_area_property() const {
    return has_hide_status_area_property_;
  }
  bool should_show_layout_mode_button() const {
    return should_show_layout_mode_button_;
  }

  // static implementation for chromeos::PanelBrowserView.
  static WindowOpenDisposition DispositionForPopupBounds(
      const gfx::Rect& bounds);

 protected:
  virtual void GetAccessiblePanes(
      std::vector<views::AccessiblePaneView*>* panes) OVERRIDE;

 private:
  void InitSystemMenu();

  void ShowInternal(bool is_active);

  // Updates |has_hide_status_area_property_| by querying the X server.
  void FetchHideStatusAreaProperty();

  // Updates |should_show_layout_mode_button_|.  Changes won't be visible
  // onscreen until Layout() is called.
  void UpdateLayoutModeButtonVisibility();

  StatusAreaViewChromeos* status_area_;
  LayoutModeButton* layout_mode_button_;

  // System menu.
  scoped_ptr<views::MenuRunner> system_menu_runner_;
  scoped_ptr<views::MenuDelegate> system_menu_delegate_;

  // Focused native widget before wrench menu shows up. We need this to properly
  // perform cut, copy and paste. See http://crosbug.com/8496
  gfx::NativeView saved_focused_widget_;

  // Is the _CHROME_STATE_STATUS_HIDDEN atom present in our toplevel window's
  // _CHROME_STATE X property?  This gets set by window manager to tell us to
  // hide the status area.
  bool has_hide_status_area_property_;

  // Should the layout mode button be shown?  We only show it if there are
  // multiple browsers open.
  bool should_show_layout_mode_button_;

  DISALLOW_COPY_AND_ASSIGN(BrowserView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_
