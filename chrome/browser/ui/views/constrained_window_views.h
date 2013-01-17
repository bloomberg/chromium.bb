// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/web_contents_modal_dialog.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"

namespace content {
class WebContents;
}
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
//  A WebContentsModalDialog implementation that implements the dialog as a
//  child HWND with a custom window frame. The ConstrainedWindowViews owns
//  itself and will be deleted soon after being closed.
//
class ConstrainedWindowViews : public views::Widget,
                               public WebContentsModalDialog,
                               public NativeConstrainedWindowDelegate {
 public:
  ConstrainedWindowViews(content::WebContents* web_contents,
                         views::WidgetDelegate* widget_delegate);
  virtual ~ConstrainedWindowViews();

  // Returns the WebContents that constrains this Constrained Window.
  content::WebContents* owner() const { return web_contents_; }

  // Overridden from WebContentsModalDialog:
  virtual void ShowWebContentsModalDialog() OVERRIDE;
  virtual void CloseWebContentsModalDialog() OVERRIDE;
  virtual void FocusWebContentsModalDialog() OVERRIDE;
  virtual void PulseWebContentsModalDialog() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;

  // Default insets for the dialog:
  static gfx::Insets GetDefaultInsets();

 private:
  // Overridden from views::Widget:
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  // Overridden from NativeConstrainedWindowDelegate:
  virtual void OnNativeConstrainedWindowDestroyed() OVERRIDE;
  virtual void OnNativeConstrainedWindowMouseActivate() OVERRIDE;
  virtual views::internal::NativeWidgetDelegate*
      AsNativeWidgetDelegate() OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) OVERRIDE;

  content::WebContents* web_contents_;

  NativeConstrainedWindow* native_constrained_window_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
