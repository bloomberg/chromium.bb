// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/constrained_window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "views/widget/widget.h"

class TabContentsWrapper;

namespace views {
namespace internal {
class NativeWidgetDelegate;
}
class NativeWidget;
class NonClientFrameView;
class WidgetDelegate;
}

class NativeConstrainedWindowDelegate {
 public:
  virtual ~NativeConstrainedWindowDelegate() {}

  // Called after the NativeConstrainedWindow has been destroyed and is about to
  // be deleted.
  virtual void OnNativeConstrainedWindowDestroyed() = 0;

  // Called when the NativeConstrainedWindow is clicked on when inactive.
  virtual void OnNativeConstrainedWindowMouseActivate() = 0;

  virtual views::internal::NativeWidgetDelegate* AsNativeWidgetDelegate() = 0;
};

class NativeConstrainedWindow {
 public:
  virtual ~NativeConstrainedWindow() {}

  // Creates a platform-specific implementation of NativeConstrainedWindow.
  static NativeConstrainedWindow* CreateNativeConstrainedWindow(
      NativeConstrainedWindowDelegate* delegate);

  virtual views::NativeWidget* AsNativeWidget() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowViews
//
//  A ConstrainedWindow implementation that implements a Constrained Window as
//  a child HWND with a custom window frame.
//
class ConstrainedWindowViews : public views::Widget,
                               public ConstrainedWindow,
                               public NativeConstrainedWindowDelegate {
 public:
  ConstrainedWindowViews(TabContentsWrapper* wrapper,
                         views::WidgetDelegate* widget_delegate);
  virtual ~ConstrainedWindowViews();

  // Returns the TabContentsWrapper that constrains this Constrained Window.
  TabContentsWrapper* owner() const { return wrapper_; }

  // Overridden from ConstrainedWindow:
  virtual void ShowConstrainedWindow() OVERRIDE;
  virtual void CloseConstrainedWindow() OVERRIDE;
  virtual void FocusConstrainedWindow() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;

 private:
  // Overridden from views::Widget:
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  // Overridden from NativeConstrainedWindowDelegate:
  virtual void OnNativeConstrainedWindowDestroyed() OVERRIDE;
  virtual void OnNativeConstrainedWindowMouseActivate() OVERRIDE;
  virtual views::internal::NativeWidgetDelegate*
      AsNativeWidgetDelegate() OVERRIDE;

  TabContentsWrapper* wrapper_;

  NativeConstrainedWindow* native_constrained_window_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
