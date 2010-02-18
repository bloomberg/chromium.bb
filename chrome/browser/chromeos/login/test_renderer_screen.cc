// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test_renderer_screen.h"

#include "chrome/browser/chromeos/login/rounded_rect_painter.h"

namespace {
const int kCornerRadius = 12;
const SkColor kBackground = SK_ColorWHITE;
const int kShadow = 10;
const SkColor kShadowColor = 0x40223673;
}  // namespace

TestRendererScreen::TestRendererScreen(chromeos::ScreenObserver* observer) {
}

TestRendererScreen::~TestRendererScreen() {
}

void TestRendererScreen::Init() {
  // Use rounded rect background.
  views::Painter* painter = new chromeos::RoundedRectPainter(
      0, 0x00000000,              // no padding
      kShadow, kShadowColor,      // gradient shadow
      kCornerRadius,              // corner radius
      kBackground, kBackground);  // backgound without gradient
  set_background(
      views::Background::CreateBackgroundPainter(true, painter));
}

void TestRendererScreen::UpdateLocalizedStrings() {
}
