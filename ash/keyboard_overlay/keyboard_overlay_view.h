// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_OVERLAY_KEYBOARD_OVERLAY_VIEW_H_
#define ASH_KEYBOARD_OVERLAY_KEYBOARD_OVERLAY_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/overlay_event_filter.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/webview/web_dialog_view.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace ui {
class WebDialogDelegate;
}

// A customized dialog view for the keyboard overlay.
class ASH_EXPORT KeyboardOverlayView
    : public views::WebDialogView,
      public ash::internal::OverlayEventFilter::Delegate {
 public:
  KeyboardOverlayView(content::BrowserContext* context,
                      ui::WebDialogDelegate* delegate,
                      WebContentsHandler* handler);
  virtual ~KeyboardOverlayView();

  // Overridden from ash::internal::OverlayEventFilter::Delegate:
  virtual void Cancel() OVERRIDE;
  virtual bool IsCancelingKeyEvent(aura::KeyEvent* event) OVERRIDE;
  virtual aura::Window* GetWindow() OVERRIDE;

  // Shows the keyboard overlay.
  static void ShowDialog(content::BrowserContext* context,
                         WebContentsHandler* handler,
                         const GURL& url);

 private:
  // Overridden from views::WidgetDelegate:
  virtual void WindowClosing() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayView);
};

#endif  // ASH_KEYBOARD_OVERLAY_KEYBOARD_OVERLAY_VIEW_H_
