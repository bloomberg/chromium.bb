// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
#pragma once

#include "chrome/browser/ui/extensions/shell_window.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_sk_region.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget_delegate.h"

class Profile;

class ShellWindowViews : public ShellWindow,
                         public views::NativeViewHost,
                         public views::WidgetDelegate {
 public:
  ShellWindowViews(Profile* profile,
                   const Extension* extension,
                   const GURL& url);

  // BaseWindow implementation.
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
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

  // Overridden from views::NativeViewHost:
  virtual gfx::NativeCursor GetCursor(const views::MouseEvent& event) OVERRIDE;
  virtual void SetVisible(bool is_visible) OVERRIDE;
  virtual void ViewHierarchyChanged(
      bool is_add, views::View *parent, views::View *child) OVERRIDE;

 protected:
  // Overridden from views::View.
  virtual void PreferredSizeChanged() OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e) OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  virtual ~ShellWindowViews();

  void OnViewWasResized();

  bool initialized_;
  views::Widget* window_;

  gfx::ScopedSkRegion caption_region_;

  bool use_native_frame_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
