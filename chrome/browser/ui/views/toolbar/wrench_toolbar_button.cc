// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/wrench_toolbar_button.h"

#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/views/painter.h"

WrenchToolbarButton::WrenchToolbarButton(views::MenuButtonListener* listener)
    : views::MenuButton(NULL, base::string16(), listener, false) {
  wrench_icon_painter_.reset(new WrenchIconPainter(this));
}

WrenchToolbarButton::~WrenchToolbarButton() {
}

void WrenchToolbarButton::SetSeverity(WrenchIconPainter::Severity severity,
                                      bool animate) {
  wrench_icon_painter_->SetSeverity(severity, animate);
  SchedulePaint();
}

gfx::Size WrenchToolbarButton::GetPreferredSize() const {
  return ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_TOOLBAR_BEZEL_HOVER)->size();
}

void WrenchToolbarButton::OnPaint(gfx::Canvas* canvas) {
  views::MenuButton::OnPaint(canvas);
  wrench_icon_painter_->Paint(canvas,
                              GetThemeProvider(),
                              gfx::Rect(size()),
                              WrenchIconPainter::BEZEL_NONE);
}

void WrenchToolbarButton::ScheduleWrenchIconPaint() {
  SchedulePaint();
}
