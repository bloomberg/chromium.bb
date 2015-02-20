// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_

#include "base/memory/scoped_ptr.h"
#include "extensions/components/native_app_window/native_app_window_views.h"
#include "ui/views/context_menu_controller.h"

namespace apps {
class AppWindowFrameView;
}

#if defined(USE_ASH)
namespace ash {
class ImmersiveFullscreenController;
}
#endif

class DesktopKeyboardCapture;
class ExtensionKeybindingRegistryViews;

namespace views {
class MenuRunner;
}

class ChromeNativeAppWindowViews
    : public native_app_window::NativeAppWindowViews,
      public views::ContextMenuController {
 public:
  ChromeNativeAppWindowViews();
  ~ChromeNativeAppWindowViews() override;

  SkRegion* shape() { return shape_.get(); }

 protected:
  // Called before views::Widget::Init() to allow subclasses to customize
  // the InitParams that would be passed.
  virtual void OnBeforeWidgetInit(views::Widget::InitParams* init_params,
                                  views::Widget* widget);

  virtual void InitializeDefaultWindow(
      const extensions::AppWindow::CreateParams& create_params);
  virtual void InitializePanelWindow(
      const extensions::AppWindow::CreateParams& create_params);
  virtual views::NonClientFrameView* CreateStandardDesktopAppFrame();

  // ui::BaseWindow implementation.
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  bool IsAlwaysOnTop() const override;

  // Overridden from views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& p,
                              ui::MenuSourceType source_type) override;

  // WidgetDelegate implementation.
  gfx::ImageSkia GetWindowAppIcon() override;
  gfx::ImageSkia GetWindowIcon() override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(gfx::Path* mask) const override;

  // views::View implementation.
  gfx::Size GetPreferredSize() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // NativeAppWindow implementation.
  void SetFullscreen(int fullscreen_types) override;
  bool IsFullscreenOrPending() const override;
  void UpdateBadgeIcon() override;
  void UpdateShape(scoped_ptr<SkRegion> region) override;
  bool HasFrameColor() const override;
  SkColor ActiveFrameColor() const override;
  SkColor InactiveFrameColor() const override;
  void SetInterceptAllKeys(bool want_all_keys) override;

  // NativeAppWindowViews implementation.
  void InitializeWindow(
      extensions::AppWindow* app_window,
      const extensions::AppWindow::CreateParams& create_params) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ShapedAppWindowTargeterTest,
                           ResizeInsetsWithinBounds);

  apps::AppWindowFrameView* CreateNonStandardAppFrame();

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

  // Used to capture all keyboard events including task switching sequence.
  scoped_ptr<DesktopKeyboardCapture> desktop_keyboard_capture_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_
