// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_

#include "base/macros.h"
#include "ui/gfx/display_observer.h"
#include "ui/views/widget/widget_delegate.h"

class ChromeVoxPanelWebContentsObserver;

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

class ChromeVoxPanel : public views::WidgetDelegate,
                       public gfx::DisplayObserver {
 public:
  explicit ChromeVoxPanel(content::BrowserContext* browser_context);
  ~ChromeVoxPanel() override;

  aura::Window* GetRootWindow();

  void Close();
  void DidFirstVisuallyNonEmptyPaint();
  void EnterFullscreen();
  void ExitFullscreen();
  void DisableSpokenFeedback();

  // WidgetDelegate overrides.
  const views::Widget* GetWidget() const override;
  views::Widget* GetWidget() override;
  void DeleteDelegate() override;
  views::View* GetContentsView() override;

  // DisplayObserver overrides;
  void OnDisplayAdded(const gfx::Display& new_display) override {}
  void OnDisplayRemoved(const gfx::Display& old_display) override {}
  void OnDisplayMetricsChanged(const gfx::Display& display,
                               uint32_t changed_metrics) override;

 private:
  void UpdateWidgetBounds();

  views::Widget* widget_;
  scoped_ptr<ChromeVoxPanelWebContentsObserver> web_contents_observer_;
  views::View* web_view_;
  bool fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(ChromeVoxPanel);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_
