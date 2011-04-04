// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/browser/tab_contents/constrained_window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

class ConstrainedTabContentsWindowDelegate;
class ConstrainedWindowAnimation;
class ConstrainedWindowFrameView;
namespace views {
class NativeWindow;
class NonClientFrameView;
class Window;
class WindowDelegate;
}

class NativeConstrainedWindowDelegate {
 public:
  virtual ~NativeConstrainedWindowDelegate() {}

  // Called after the NativeConstrainedWindow has been destroyed and is about to
  // be deleted.
  virtual void OnNativeConstrainedWindowDestroyed() = 0;

  // Called when the NativeConstrainedWindow is clicked on when inactive.
  virtual void OnNativeConstrainedWindowMouseActivate() = 0;

  // Creates the frame view for the constrained window.
  // TODO(beng): remove once ConstrainedWindowViews is-a views::Window.
  virtual views::NonClientFrameView* CreateFrameViewForWindow() = 0;
};

class NativeConstrainedWindow {
 public:
  virtual ~NativeConstrainedWindow() {}

  // Creates a platform-specific implementation of NativeConstrainedWindow.
  // TODO(beng): Remove WindowDelegate param once ConstrainedWindowViews is-a
  //             views::Window.
  static NativeConstrainedWindow* CreateNativeConstrainedWindow(
      NativeConstrainedWindowDelegate* delegate,
      views::WindowDelegate* window_delegate);

  virtual void InitNativeConstrainedWindow(gfx::NativeView parent) = 0;

  virtual views::NativeWindow* AsNativeWindow() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowViews
//
//  A ConstrainedWindow implementation that implements a Constrained Window as
//  a child HWND with a custom window frame.
//
class ConstrainedWindowViews : public ConstrainedWindow,
                               public NativeConstrainedWindowDelegate {
 public:
  ConstrainedWindowViews(TabContents* owner,
                         views::WindowDelegate* window_delegate);
  virtual ~ConstrainedWindowViews();

  // Returns the TabContents that constrains this Constrained Window.
  TabContents* owner() const { return owner_; }

  views::Window* GetWindow();

  // Overridden from ConstrainedWindow:
  virtual void ShowConstrainedWindow() OVERRIDE;
  virtual void CloseConstrainedWindow() OVERRIDE;
  virtual void FocusConstrainedWindow() OVERRIDE;

 private:
  // Overridden from NativeConstrainedWindowDelegate:
  virtual void OnNativeConstrainedWindowDestroyed() OVERRIDE;
  virtual void OnNativeConstrainedWindowMouseActivate() OVERRIDE;
  virtual views::NonClientFrameView* CreateFrameViewForWindow() OVERRIDE;

  // The TabContents that owns and constrains this ConstrainedWindow.
  TabContents* owner_;

  NativeConstrainedWindow* native_constrained_window_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
