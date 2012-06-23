// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
#pragma once

#include "chrome/browser/ui/extensions/shell_window.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_sk_region.h"
#include "ui/views/widget/widget_delegate.h"

class Profile;

namespace extensions {
class Extension;
}

namespace views {
class WebView;
}

class ShellWindowViews : public ShellWindow,
                         public views::WidgetDelegateView {
 public:
  ShellWindowViews(Profile* profile,
                   const extensions::Extension* extension,
                   const GURL& url,
                   const CreateParams& params);

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
  virtual void SetDraggableRegion(SkRegion* region) OVERRIDE;
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

 protected:
  // views::View implementation.
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(
      bool is_add, views::View *parent, views::View *child) OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual gfx::Size GetMaximumSize() OVERRIDE;
  virtual void OnFocus() OVERRIDE;

  // ShellWindow implementation.
  virtual void UpdateWindowTitle() OVERRIDE;
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreenOrPending() const OVERRIDE;

 private:
  friend class ShellWindowFrameView;

  virtual ~ShellWindowViews();

  void OnViewWasResized();

  views::WebView* web_view_;
  views::Widget* window_;
  bool is_fullscreen_;

  gfx::ScopedSkRegion caption_region_;

  bool use_custom_frame_;
  gfx::Size minimum_size_;
  gfx::Size maximum_size_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
