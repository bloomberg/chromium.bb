// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_

#include <vector>

#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/views/frame/browser_view.h"

class AccessibleToolbarView;
class TabStripModel;

namespace menus {
class SimpleMenuModel;
}  // namespace menus

namespace views {
class ImageButton;
class ImageView;
class Menu2;
}  // namespace views

class Profile;

namespace chromeos {

class StatusAreaView;
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
  explicit BrowserView(Browser* browser);
  virtual ~BrowserView();

  // BrowserView overrides.
  virtual void Init();
  virtual void Show();
  virtual void FocusChromeOSStatus();
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
  virtual bool IsBrowserMode() const;
  virtual bool IsScreenLockerMode() const;

 protected:
  virtual void GetAccessibleToolbars(
      std::vector<AccessibleToolbarView*>* toolbars);

 private:
  void InitSystemMenu();

  // Updates the background of the otr icon. The background differs for vertical
  // tabs.
  void UpdateOTRBackground();

  // Status Area view.
  StatusAreaView* status_area_;

  // System menus.
  scoped_ptr<menus::SimpleMenuModel> system_menu_contents_;
  scoped_ptr<views::Menu2> system_menu_menu_;

  // A flag to specify if the browser window should be maximized.
  bool force_maximized_window_;

  // Off the record icon.
  views::ImageView* otr_avatar_icon_;

  DISALLOW_COPY_AND_ASSIGN(BrowserView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_VIEW_H_
