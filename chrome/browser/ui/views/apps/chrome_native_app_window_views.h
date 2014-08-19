// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_

#include "apps/ui/views/native_app_window_views.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/context_menu_controller.h"

#if defined(USE_ASH)
namespace ash {
class ImmersiveFullscreenController;
}
#endif

class ExtensionKeybindingRegistryViews;

namespace views {
class MenuRunner;
}

class ChromeNativeAppWindowViews : public apps::NativeAppWindowViews,
                                   public views::ContextMenuController {
 public:
  ChromeNativeAppWindowViews();
  virtual ~ChromeNativeAppWindowViews();

  SkRegion* shape() { return shape_.get(); }

 protected:
  // Called before views::Widget::Init() to allow subclasses to customize
  // the InitParams that would be passed.
  virtual void OnBeforeWidgetInit(views::Widget::InitParams* init_params,
                                  views::Widget* widget);

  virtual void InitializeDefaultWindow(
      const apps::AppWindow::CreateParams& create_params);
  virtual void InitializePanelWindow(
      const apps::AppWindow::CreateParams& create_params);
  virtual views::NonClientFrameView* CreateStandardDesktopAppFrame();

 private:
  FRIEND_TEST_ALL_PREFIXES(ShapedAppWindowTargeterTest,
                           ResizeInsetsWithinBounds);

  apps::AppWindowFrameView* CreateNonStandardAppFrame();

  // ui::BaseWindow implementation.
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual ui::WindowShowState GetRestoredState() const OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;

  // Overridden from views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& p,
                                      ui::MenuSourceType source_type) OVERRIDE;

  // WidgetDelegate implementation.
  virtual gfx::ImageSkia GetWindowAppIcon() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE;
  virtual bool WidgetHasHitTestMask() const OVERRIDE;
  virtual void GetWidgetHitTestMask(gfx::Path* mask) const OVERRIDE;

  // views::View implementation.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // NativeAppWindow implementation.
  virtual void SetFullscreen(int fullscreen_types) OVERRIDE;
  virtual bool IsFullscreenOrPending() const OVERRIDE;
  virtual void UpdateBadgeIcon() OVERRIDE;
  virtual void UpdateShape(scoped_ptr<SkRegion> region) OVERRIDE;
  virtual bool HasFrameColor() const OVERRIDE;
  virtual SkColor ActiveFrameColor() const OVERRIDE;
  virtual SkColor InactiveFrameColor() const OVERRIDE;

  // NativeAppWindowViews implementation.
  virtual void InitializeWindow(
      apps::AppWindow* app_window,
      const apps::AppWindow::CreateParams& create_params) OVERRIDE;

  // True if the window is fullscreen or fullscreen is pending.
  bool is_fullscreen_;

  // Custom shape of the window. If this is not set then the window has a
  // default shape, usually rectangular.
  scoped_ptr<SkRegion> shape_;

  bool has_frame_color_;
  SkColor active_frame_color_;
  SkColor inactive_frame_color_;
  gfx::Size preferred_size_;

  // The class that registers for keyboard shortcuts for extension commands.
  scoped_ptr<ExtensionKeybindingRegistryViews> extension_keybinding_registry_;

#if defined(USE_ASH)
  // Used to put non-frameless windows into immersive fullscreen on ChromeOS. In
  // immersive fullscreen, the window header (title bar and window controls)
  // slides onscreen as an overlay when the mouse is hovered at the top of the
  // screen.
  scoped_ptr<ash::ImmersiveFullscreenController>
      immersive_fullscreen_controller_;
#endif  // defined(USE_ASH)

  // Used to show the system menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_
