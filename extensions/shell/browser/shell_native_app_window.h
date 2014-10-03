// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_NATIVE_APP_WINDOW_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_NATIVE_APP_WINDOW_H_

#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"

namespace extensions {

// app_shell's NativeAppWindow implementation.
class ShellNativeAppWindow : public NativeAppWindow {
 public:
  ShellNativeAppWindow(AppWindow* app_window,
                       const AppWindow::CreateParams& params);
  virtual ~ShellNativeAppWindow();

  // ui::BaseView overrides:
  virtual bool IsActive() const override;
  virtual bool IsMaximized() const override;
  virtual bool IsMinimized() const override;
  virtual bool IsFullscreen() const override;
  virtual gfx::NativeWindow GetNativeWindow() override;
  virtual gfx::Rect GetRestoredBounds() const override;
  virtual ui::WindowShowState GetRestoredState() const override;
  virtual gfx::Rect GetBounds() const override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void ShowInactive() override;
  virtual void Close() override;
  virtual void Activate() override;
  virtual void Deactivate() override;
  virtual void Maximize() override;
  virtual void Minimize() override;
  virtual void Restore() override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual void FlashFrame(bool flash) override;
  virtual bool IsAlwaysOnTop() const override;
  virtual void SetAlwaysOnTop(bool always_on_top) override;

  // web_modal::ModalDialogHost overrides:
  virtual gfx::NativeView GetHostView() const override;
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) override;
  virtual void AddObserver(
      web_modal::ModalDialogHostObserver* observer) override;
  virtual void RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) override;

  // web_modal::WebContentsModalDialogHost overrides:
  virtual gfx::Size GetMaximumDialogSize() override;

  // NativeAppWindow overrides:
  virtual void SetFullscreen(int fullscreen_types) override;
  virtual bool IsFullscreenOrPending() const override;
  virtual void UpdateWindowIcon() override;
  virtual void UpdateWindowTitle() override;
  virtual void UpdateBadgeIcon() override;
  virtual void UpdateDraggableRegions(
      const std::vector<DraggableRegion>& regions) override;
  virtual SkRegion* GetDraggableRegion() override;
  virtual void UpdateShape(scoped_ptr<SkRegion> region) override;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  virtual bool IsFrameless() const override;
  virtual bool HasFrameColor() const override;
  virtual SkColor ActiveFrameColor() const override;
  virtual SkColor InactiveFrameColor() const override;
  virtual gfx::Insets GetFrameInsets() const override;
  virtual void ShowWithApp() override;
  virtual void HideWithApp() override;
  virtual void UpdateShelfMenu() override;
  virtual gfx::Size GetContentMinimumSize() const override;
  virtual gfx::Size GetContentMaximumSize() const override;
  virtual void SetContentSizeConstraints(const gfx::Size& min_size,
                                         const gfx::Size& max_size) override;
  virtual void SetVisibleOnAllWorkspaces(bool always_visible) override;
  virtual bool CanHaveAlphaEnabled() const override;

 private:
  aura::Window* GetWindow() const;

  AppWindow* app_window_;

  DISALLOW_COPY_AND_ASSIGN(ShellNativeAppWindow);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_NATIVE_APP_WINDOW_H_
