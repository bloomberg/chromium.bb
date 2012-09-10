// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_

#include "chrome/browser/ui/base_window.h"
#include "chrome/browser/ui/extensions/native_shell_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/views/unhandled_keyboard_event_handler.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget_delegate.h"

class ExtensionKeybindingRegistryViews;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
struct DraggableRegion;
}

namespace views {
class WebView;
}

class ShellWindowViews : public NativeShellWindow,
                         public views::WidgetDelegateView {
 public:
  ShellWindowViews(ShellWindow* shell_window,
                   const ShellWindow::CreateParams& params);

  bool frameless() const { return frameless_; }
  SkRegion* draggable_region() { return draggable_region_.get(); }

  // BaseWindow implementation.
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void Show() OVERRIDE;
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

  // WidgetDelegate implementation.
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) OVERRIDE;
  virtual gfx::ImageSkia GetWindowAppIcon() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;

 protected:
  // views::View implementation.
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(
      bool is_add, views::View *parent, views::View *child) OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual gfx::Size GetMaximumSize() OVERRIDE;
  virtual void OnFocus() OVERRIDE;

  Profile* profile() { return shell_window_->profile(); }
  content::WebContents* web_contents() {
    return shell_window_->web_contents();
  }
  const extensions::Extension* extension() {
    return shell_window_->extension();
  }

  // views::WidgetDelegate implementation.
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   ui::WindowShowState show_state) OVERRIDE;

 private:
  friend class ShellWindowFrameView;

  virtual ~ShellWindowViews();

  // NativeShellWindow implementation.
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreenOrPending() const OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;
  virtual void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

  void OnViewWasResized();

  ShellWindow* shell_window_; // weak - ShellWindow owns NativeShellWindow.

  views::WebView* web_view_;
  views::Widget* window_;
  bool is_fullscreen_;

  scoped_ptr<SkRegion> draggable_region_;

  bool frameless_;
  gfx::Size minimum_size_;
  gfx::Size maximum_size_;

  // The class that registers for keyboard shortcuts for extension commands.
  scoped_ptr<ExtensionKeybindingRegistryViews> extension_keybinding_registry_;

  UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
