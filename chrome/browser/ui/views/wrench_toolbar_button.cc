// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/wrench_toolbar_button.h"

#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"

WrenchToolbarButton::WrenchToolbarButton(views::MenuButtonListener* listener)
    : views::MenuButton(NULL, string16(), listener, false) {
  wrench_icon_painter_.reset(new WrenchIconPainter(this));

  // Used for sizing only.
  ui::ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetIcon(*rb.GetImageSkiaNamed(IDR_TOOLBAR_BEZEL_HOVER));
}

WrenchToolbarButton::~WrenchToolbarButton() {
}

void WrenchToolbarButton::SetSeverity(WrenchIconPainter::Severity severity) {
  wrench_icon_painter_->SetSeverity(severity);
}

void WrenchToolbarButton::OnPaint(gfx::Canvas* canvas) {
  // Badge non-ChromeOS ASH builds so that users know that they are using ASH.
#if defined(USE_ASH) && !defined(OS_CHROMEOS)
  wrench_icon_painter_->set_badge(
      *GetThemeProvider()->GetImageSkiaNamed(IDR_TOOLS_BADGE_ASH));
#endif

  wrench_icon_painter_->Paint(
      canvas, GetThemeProvider(), gfx::Rect(size()), GetCurrentBezelType());
}

void WrenchToolbarButton::ScheduleWrenchIconPaint() {
  SchedulePaint();
}

WrenchIconPainter::BezelType WrenchToolbarButton::GetCurrentBezelType() const {
  switch (state()) {
    case STATE_HOVERED:
      return WrenchIconPainter::BEZEL_HOVER;
    case STATE_PRESSED:
      return WrenchIconPainter::BEZEL_PRESSED;
    default:
      return WrenchIconPainter::BEZEL_NONE;
  }
}
