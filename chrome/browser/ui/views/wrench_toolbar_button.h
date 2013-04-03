// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WRENCH_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WRENCH_TOOLBAR_BUTTON_H_

#include "chrome/browser/ui/toolbar/wrench_icon_painter.h"
#include "ui/views/controls/button/menu_button.h"

class WrenchToolbarButton : public views::MenuButton,
                            public WrenchIconPainter::Delegate {
 public:
  explicit WrenchToolbarButton(views::MenuButtonListener* menu_button_listener);
  virtual ~WrenchToolbarButton();

  void SetSeverity(WrenchIconPainter::Severity severity);

 private:
  // views::MenuButton:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // WrenchIconPainter::Delegate:
  virtual void ScheduleWrenchIconPaint() OVERRIDE;

  WrenchIconPainter::BezelType GetCurrentBezelType() const;

  scoped_ptr<WrenchIconPainter> wrench_icon_painter_;

  DISALLOW_COPY_AND_ASSIGN(WrenchToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WRENCH_TOOLBAR_BUTTON_H_
