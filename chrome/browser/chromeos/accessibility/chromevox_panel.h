// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/widget_delegate.h"

class ChromeVoxPanelWebContentsObserver;

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

class ChromeVoxPanel : public views::WidgetDelegate,
                       public display::DisplayObserver {
 public:
  explicit ChromeVoxPanel(content::BrowserContext* browser_context);
  ~ChromeVoxPanel() override;

  aura::Window* GetRootWindow();

  void Close();
  void DidFirstVisuallyNonEmptyPaint();
  void UpdatePanelHeight();
  void EnterFullscreen();
  void ExitFullscreen();
  void DisableSpokenFeedback();
  void Focus();
  void UpdateWidgetBounds();

  // WidgetDelegate overrides.
  const views::Widget* GetWidget() const override;
  views::Widget* GetWidget() override;
  void DeleteDelegate() override;
  views::View* GetContentsView() override;

  // DisplayObserver overrides;
  void OnDisplayAdded(const display::Display& new_display) override {}
  void OnDisplayRemoved(const display::Display& old_display) override {}
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  views::Widget* widget_;
  std::unique_ptr<ChromeVoxPanelWebContentsObserver> web_contents_observer_;
  views::View* web_view_;
  bool panel_fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(ChromeVoxPanel);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_
