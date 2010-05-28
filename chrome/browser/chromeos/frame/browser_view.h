// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_

#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/views/frame/browser_view.h"

class TabStripModel;

namespace menus {
class SimpleMenuModel;
}  // namespace menus

namespace views {
class ImageButton;
class Menu2;
}  // namespace views

class Profile;

namespace chromeos {

class BrowserStatusAreaView;
class CompactLocationBar;
class CompactLocationBarHost;
class CompactNavigationBar;
class StatusAreaButton;

// chromeos::BrowserView adds ChromeOS specific controls and menus to a
// BrowserView created with Browser::TYPE_NORMAL. This extender adds controls
// to the title bar as follows:
//       ____  __ __
//      /    \   \  \     [StatusArea]
//
// and adds the system context menu to the remaining arae of the titlebar.
class BrowserView : public ::BrowserView,
                    public views::ContextMenuController,
                    public StatusAreaHost {
 public:
  // There are three distinct ui styles:
  // . Standards uses the same layout as chrome. Within standard the user can
  //   turn on side tabs. Side tabs are still represented by the constant
  //   StandardStyle.
  // . Compact mode hides the omnibox/toolbar to save the vertical real estate,
  //   and uses QSB (compact nav bar) to launch/switch url.
  enum UIStyle {
    StandardStyle = 0,
    CompactStyle,
  };

  explicit BrowserView(Browser* browser);
  virtual ~BrowserView();

  // BrowserView overrides.
  virtual void Init();
  virtual void Show();
  virtual bool IsToolbarVisible() const;
  virtual void SetFocusToLocationBar(bool select_all);
  virtual void ToggleCompactNavigationBar();
  virtual views::LayoutManager* CreateLayoutManager() const;
  virtual void InitTabStrip(TabStripModel* tab_strip_model);
  virtual void ChildPreferredSizeChanged(View* child);
  virtual bool GetSavedWindowBounds(gfx::Rect* bounds) const;

  // views::ContextMenuController overrides.
  virtual void ShowContextMenu(views::View* source,
                               const gfx::Point& p,
                               bool is_mouse_gesture);

  // StatusAreaHost overrides.
  virtual Profile* GetProfile() const;
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual bool ShouldOpenButtonOptions(
      const views::View* button_view) const;
  virtual void ExecuteBrowserCommand(int id) const;
  virtual void OpenButtonOptions(const views::View* button_view) const;
  virtual bool IsButtonVisible(const views::View* button_view) const;
  virtual bool IsBrowserMode() const;

  // Shows the compact location bar under the selected tab.
  void ShowCompactLocationBarUnderSelectedTab(bool select_all);

  // Returns true if the ui style is in Compact mode.
  bool is_compact_style() const {
    return ui_style_ == CompactStyle;
  }

 private:
  friend class CompactLocationBarHostTest;

  CompactLocationBarHost* compact_location_bar_host() {
    return compact_location_bar_host_.get();
  }

  void InitSystemMenu();

  // Updates the background of the otr icon. The background differs for vertical
  // tabs.
  void UpdateOTRBackground();

  // Status Area view.
  BrowserStatusAreaView* status_area_;

  // System menus.
  scoped_ptr<menus::SimpleMenuModel> system_menu_contents_;
  scoped_ptr<views::Menu2> system_menu_menu_;

  // CompactNavigationBar view.
  CompactNavigationBar* compact_navigation_bar_;

  // The current UI style of the browser.
  UIStyle ui_style_;

  // CompactLocationBarHost.
  scoped_ptr<CompactLocationBarHost> compact_location_bar_host_;

  // A flag to specify if the browser window should be maximized.
  bool force_maximized_window_;

  // A spacer under the tap strip used when the compact navigation bar
  // is active.
  views::View* spacer_;

  // Off the record icon.
  views::ImageView* otr_avatar_icon_;

  // Menu button shown in status area when browser is in compact mode.
  StatusAreaButton* menu_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_
