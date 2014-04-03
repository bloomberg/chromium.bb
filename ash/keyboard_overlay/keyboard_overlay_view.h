// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_OVERLAY_KEYBOARD_OVERLAY_VIEW_H_
#define ASH_KEYBOARD_OVERLAY_KEYBOARD_OVERLAY_VIEW_H_

#include <vector>

#include "ash/content_support/ash_with_content_export.h"
#include "ash/wm/overlay_event_filter.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "ui/views/controls/webview/web_dialog_view.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace ui {
class WebDialogDelegate;
}

namespace ash {

// A customized dialog view for the keyboard overlay.
class ASH_WITH_CONTENT_EXPORT KeyboardOverlayView
    : public views::WebDialogView,
      public ash::OverlayEventFilter::Delegate {
 public:
  struct KeyEventData {
    ui::KeyboardCode key_code;
    int flags;
  };

  KeyboardOverlayView(content::BrowserContext* context,
                      ui::WebDialogDelegate* delegate,
                      WebContentsHandler* handler);
  virtual ~KeyboardOverlayView();

  // Overridden from ash::OverlayEventFilter::Delegate:
  virtual void Cancel() OVERRIDE;
  virtual bool IsCancelingKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual aura::Window* GetWindow() OVERRIDE;

  // Shows the keyboard overlay.
  static void ShowDialog(content::BrowserContext* context,
                         WebContentsHandler* handler,
                         const GURL& url);

 private:
  FRIEND_TEST_ALL_PREFIXES(KeyboardOverlayViewTest, OpenAcceleratorsClose);
  FRIEND_TEST_ALL_PREFIXES(KeyboardOverlayViewTest, NoRedundantCancelingKeys);

  // Overridden from views::WidgetDelegate:
  virtual void WindowClosing() OVERRIDE;

  static void GetCancelingKeysForTesting(
      std::vector<KeyEventData>* canceling_keys);

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayView);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_OVERLAY_KEYBOARD_OVERLAY_VIEW_H_
