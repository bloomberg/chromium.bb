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
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual ui::WindowShowState GetRestoredState() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;

  // web_modal::ModalDialogHost overrides:
  virtual gfx::NativeView GetHostView() const OVERRIDE;
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) OVERRIDE;
  virtual void AddObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;

  // web_modal::WebContentsModalDialogHost overrides:
  virtual gfx::Size GetMaximumDialogSize() OVERRIDE;

  // NativeAppWindow overrides:
  virtual void SetFullscreen(int fullscreen_types) OVERRIDE;
  virtual bool IsFullscreenOrPending() const OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;
  virtual void UpdateBadgeIcon() OVERRIDE;
  virtual void UpdateDraggableRegions(
      const std::vector<DraggableRegion>& regions) OVERRIDE;
  virtual SkRegion* GetDraggableRegion() OVERRIDE;
  virtual void UpdateShape(scoped_ptr<SkRegion> region) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool IsFrameless() const OVERRIDE;
  virtual bool HasFrameColor() const OVERRIDE;
  virtual SkColor ActiveFrameColor() const OVERRIDE;
  virtual SkColor InactiveFrameColor() const OVERRIDE;
  virtual gfx::Insets GetFrameInsets() const OVERRIDE;
  virtual void ShowWithApp() OVERRIDE;
  virtual void HideWithApp() OVERRIDE;
  virtual void UpdateShelfMenu() OVERRIDE;
  virtual gfx::Size GetContentMinimumSize() const OVERRIDE;
  virtual gfx::Size GetContentMaximumSize() const OVERRIDE;
  virtual void SetContentSizeConstraints(const gfx::Size& min_size,
                                         const gfx::Size& max_size) OVERRIDE;
  virtual void SetVisibleOnAllWorkspaces(bool always_visible) OVERRIDE;
  virtual bool CanHaveAlphaEnabled() const OVERRIDE;

 private:
  aura::Window* GetWindow() const;

  AppWindow* app_window_;

  DISALLOW_COPY_AND_ASSIGN(ShellNativeAppWindow);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_NATIVE_APP_WINDOW_H_
