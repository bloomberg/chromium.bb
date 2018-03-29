// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

// Displays spoken feedback UI controls for the ChromeVox component extension
// in a small panel at the top of the display. Widget bounds are managed by ash.
class ChromeVoxPanel : public views::WidgetDelegate {
 public:
  ChromeVoxPanel(content::BrowserContext* browser_context,
                 bool for_blocked_user_session);
  ~ChromeVoxPanel() override;

  aura::Window* GetRootWindow();

  // Closes the panel immediately, deleting the WebView/WebContents.
  void CloseNow();

  // Closes the panel asynchronously.
  void Close();

  // WidgetDelegate overrides.
  const views::Widget* GetWidget() const override;
  views::Widget* GetWidget() override;
  void DeleteDelegate() override;
  views::View* GetContentsView() override;

  bool for_blocked_user_session() const { return for_blocked_user_session_; }

 private:
  class ChromeVoxPanelWebContentsObserver;

  // Methods indirectly invoked by the component extension.
  void DidFirstVisuallyNonEmptyPaint();
  void EnterFullscreen();
  void ExitFullscreen();
  void Focus();

  // Sends a request to the ash window manager.
  void SetAccessibilityPanelFullscreen(bool fullscreen);

  views::Widget* widget_;
  std::unique_ptr<ChromeVoxPanelWebContentsObserver> web_contents_observer_;
  views::View* web_view_;
  const bool for_blocked_user_session_;

  DISALLOW_COPY_AND_ASSIGN(ChromeVoxPanel);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_
