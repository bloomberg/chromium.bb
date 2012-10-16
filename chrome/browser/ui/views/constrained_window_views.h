// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/constrained_window.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
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
//  A ConstrainedWindow implementation that implements a Constrained Window as
//  a child HWND with a custom window frame. The ConstrainedWindowViews owns
//  itself and will be deleted soon after being closed.
//
class ConstrainedWindowViews : public views::Widget,
                               public ConstrainedWindow,
                               public NativeConstrainedWindowDelegate,
                               public content::WebContentsObserver,
                               public content::NotificationObserver {
 public:
  // Types of insets to use with chrome style frame.
  enum ChromeStyleClientInsets {
    DEFAULT_INSETS,
    NO_INSETS,
  };

  ConstrainedWindowViews(content::WebContents* web_contents,
                         views::WidgetDelegate* widget_delegate,
                         bool enable_chrome_style,
                         ChromeStyleClientInsets chrome_style_client_insets);
  virtual ~ConstrainedWindowViews();

  // Returns the WebContents that constrains this Constrained Window.
  content::WebContents* owner() const { return web_contents_; }

  // Overridden from ConstrainedWindow:
  virtual void ShowConstrainedWindow() OVERRIDE;
  virtual void CloseConstrainedWindow() OVERRIDE;
  virtual void FocusConstrainedWindow() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;

  // Overridden from views::Widget:
  void CenterWindow(const gfx::Size& size);

  // Default insets for the dialog:
  static gfx::Insets GetDefaultInsets();

 private:
  void NotifyTabHelperWillClose();

  // Overridden from views::Widget:
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  // Overridden from NativeConstrainedWindowDelegate:
  virtual void OnNativeConstrainedWindowDestroyed() OVERRIDE;
  virtual void OnNativeConstrainedWindowMouseActivate() OVERRIDE;
  virtual views::internal::NativeWidgetDelegate*
      AsNativeWidgetDelegate() OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) OVERRIDE;

  // Overridden from content::WebContentsObserver:
  virtual void WebContentsDestroyed(content::WebContents* web_contents)
      OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Set the top of the window to overlap the browser chrome.
  void PositionChromeStyleWindow(const gfx::Size& size);

  content::NotificationRegistrar registrar_;
  content::WebContents* web_contents_;

  // TODO(wittman): remove once all constrained window dialogs are moved
  // over to Chrome style.
  const bool enable_chrome_style_;

  // Client insets to use when |enable_chrome_style_| is true.
  ChromeStyleClientInsets chrome_style_client_insets_;

  NativeConstrainedWindow* native_constrained_window_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
