// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_TOOLBAR_BUTTON_H_

#include "chrome/browser/ui/toolbar/wrench_icon_painter.h"
#include "ui/views/controls/button/menu_button.h"

// TODO(gbillock): Rename this? No longer a wrench.
class WrenchToolbarButton : public views::MenuButton,
                            public WrenchIconPainter::Delegate {
 public:
  explicit WrenchToolbarButton(views::MenuButtonListener* menu_button_listener);
  virtual ~WrenchToolbarButton();

  void SetSeverity(WrenchIconPainter::Severity severity, bool animate);

  // views::MenuButton:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // WrenchIconPainter::Delegate:
  virtual void ScheduleWrenchIconPaint() OVERRIDE;

 private:
  scoped_ptr<WrenchIconPainter> wrench_icon_painter_;

  DISALLOW_COPY_AND_ASSIGN(WrenchToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_TOOLBAR_BUTTON_H_
