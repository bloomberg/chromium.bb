// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "extensions/components/native_app_window/native_app_window_views.h"

class ExtensionKeybindingRegistryViews;

class ChromeNativeAppWindowViews
    : public native_app_window::NativeAppWindowViews {
 public:
  ChromeNativeAppWindowViews();
  ~ChromeNativeAppWindowViews() override;

  SkRegion* shape() { return shape_.get(); }

 protected:
  // Called before views::Widget::Init() in InitializeDefaultWindow() to allow
  // subclasses to customize the InitParams that would be passed.
  virtual void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget);
  // Called before views::Widget::Init() in InitializeDefaultWindow() to allow
  // subclasses to customize the InitParams that would be passed.
  virtual void OnBeforePanelWidgetInit(bool use_default_bounds,
                                       views::Widget::InitParams* init_params,
                                       views::Widget* widget);

  virtual void InitializeDefaultWindow(
      const extensions::AppWindow::CreateParams& create_params);
  virtual void InitializePanelWindow(
      const extensions::AppWindow::CreateParams& create_params);
  virtual views::NonClientFrameView* CreateStandardDesktopAppFrame();
  virtual views::NonClientFrameView* CreateNonStandardAppFrame() = 0;

  // ui::BaseWindow implementation.
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  bool IsAlwaysOnTop() const override;

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
  void UpdateShape(std::unique_ptr<SkRegion> region) override;
  bool HasFrameColor() const override;
  SkColor ActiveFrameColor() const override;
  SkColor InactiveFrameColor() const override;

  // NativeAppWindowViews implementation.
  void InitializeWindow(
      extensions::AppWindow* app_window,
      const extensions::AppWindow::CreateParams& create_params) override;

 private:
  // Custom shape of the window. If this is not set then the window has a
  // default shape, usually rectangular.
  std::unique_ptr<SkRegion> shape_;

  bool has_frame_color_;
  SkColor active_frame_color_;
  SkColor inactive_frame_color_;
  gfx::Size preferred_size_;

  // The class that registers for keyboard shortcuts for extension commands.
  std::unique_ptr<ExtensionKeybindingRegistryViews>
      extension_keybinding_registry_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_H_
