// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_ASH_H_

#include <memory>

#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura.h"
#include "ui/views/context_menu_controller.h"

namespace ash {
class ImmersiveFullscreenController;
}

namespace ui {
class MenuModel;
}

namespace views {
class MenuModelAdapter;
class MenuRunner;
}

class ChromeNativeAppWindowViewsAuraAshBrowserTest;

// Ash-specific parts of ChromeNativeAppWindowViewsAura. This is used on CrOS.
class ChromeNativeAppWindowViewsAuraAsh : public ChromeNativeAppWindowViewsAura,
                                          public views::ContextMenuController,
                                          public ash::TabletModeObserver {
 public:
  ChromeNativeAppWindowViewsAuraAsh();
  ~ChromeNativeAppWindowViewsAuraAsh() override;

 protected:
  // NativeAppWindowViews:
  void InitializeWindow(
      extensions::AppWindow* app_window,
      const extensions::AppWindow::CreateParams& create_params) override;

  // ChromeNativeAppWindowViews:
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;
  void OnBeforePanelWidgetInit(views::Widget::InitParams* init_params,
                               views::Widget* widget) override;
  views::NonClientFrameView* CreateNonStandardAppFrame() override;

  // ui::BaseWindow:
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  bool IsAlwaysOnTop() const override;

  // views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& p,
                              ui::MenuSourceType source_type) override;

  // WidgetDelegate:
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;

  // NativeAppWindow:
  void SetFullscreen(int fullscreen_types) override;
  void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) override;
  void SetActivateOnPointer(bool activate_on_pointer) override;

  // ash:TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           ImmersiveWorkFlow);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           ImmersiveModeFullscreenRestoreType);
  FRIEND_TEST_ALL_PREFIXES(ShapedAppWindowTargeterTest,
                           ResizeInsetsWithinBounds);

  // Callback for MenuModelAdapter
  void OnMenuClosed();

  // Helper function which returns true if in tablet mode, the auto hide
  // titlebars feature is turned on, and the widget is maximizable.
  bool CanAutohideTitlebarsInTabletMode() const;

  // Used to put non-frameless windows into immersive fullscreen on ChromeOS. In
  // immersive fullscreen, the window header (title bar and window controls)
  // slides onscreen as an overlay when the mouse is hovered at the top of the
  // screen.
  std::unique_ptr<ash::ImmersiveFullscreenController>
      immersive_fullscreen_controller_;

  // Used to show the system menu.
  std::unique_ptr<ui::MenuModel> menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViewsAuraAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_ASH_H_
