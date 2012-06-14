// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DIALOG_VIEW_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/web_dialog_view.h"

namespace content {
class BrowserContext;
}

namespace ui {
class WebDialogDelegate;
}

// A customized dialog view for the keyboard overlay.
class KeyboardOverlayDialogView : public WebDialogView {
 public:
  KeyboardOverlayDialogView(content::BrowserContext* context,
                            ui::WebDialogDelegate* delegate);
  virtual ~KeyboardOverlayDialogView();

  // Shows the keyboard overlay.
  static void ShowDialog(content::BrowserContext* context);

 private:
  // Overridden from views::WidgetDelegate:
  virtual void WindowClosing() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DIALOG_VIEW_H_
