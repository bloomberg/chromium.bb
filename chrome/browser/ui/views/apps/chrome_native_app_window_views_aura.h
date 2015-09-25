// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"
#include "ui/views/context_menu_controller.h"

namespace apps {
class AppWindowFrameView;
}

namespace ash {
class ImmersiveFullscreenController;
}

namespace views {
class MenuRunner;
}

// Aura-specific parts of ChromeNativeAppWindowViews. This is used on Linux,
// CrOS, and Windows and implies USE_ASH.
class ChromeNativeAppWindowViewsAura : public ChromeNativeAppWindowViews,
                                       public views::ContextMenuController {
 public:
  ChromeNativeAppWindowViewsAura();
  ~ChromeNativeAppWindowViewsAura() override;

 protected:
  // NativeAppWindowViews implementation.
  void InitializeWindow(
      extensions::AppWindow* app_window,
      const extensions::AppWindow::CreateParams& create_params) override;

  // ChromeNativeAppWindowViews implementation.
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;
  void OnBeforePanelWidgetInit(bool use_default_bounds,
                               views::Widget::InitParams* init_params,
                               views::Widget* widget) override;
  views::NonClientFrameView* CreateNonStandardAppFrame() override;

  // ui::BaseWindow implementation.
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  bool IsAlwaysOnTop() const override;

  // views::ContextMenuController implementation.
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& p,
                              ui::MenuSourceType source_type) override;

  // WidgetDelegate implementation.
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;

  // NativeAppWindow implementation.
  void SetFullscreen(int fullscreen_types) override;
  void UpdateShape(scoped_ptr<SkRegion> region) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ShapedAppWindowTargeterTest,
                           ResizeInsetsWithinBounds);

  // Used to put non-frameless windows into immersive fullscreen on ChromeOS. In
  // immersive fullscreen, the window header (title bar and window controls)
  // slides onscreen as an overlay when the mouse is hovered at the top of the
  // screen.
  scoped_ptr<ash::ImmersiveFullscreenController>
      immersive_fullscreen_controller_;

#if defined(OS_CHROMEOS)
  // Used to show the system menu.
  scoped_ptr<views::MenuRunner> menu_runner_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViewsAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_H_
