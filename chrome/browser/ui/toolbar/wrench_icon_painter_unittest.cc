// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/wrench_icon_painter.h"

#include "base/message_loop.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"

class WrenchIconPainterTest : public testing::Test,
    public WrenchIconPainter::Delegate {
 public:
  WrenchIconPainterTest() : schedule_paint_count_(0), painter_(this) {
    theme_provider_ = ThemeServiceFactory::GetForProfile(&profile_);
  }

  virtual void ScheduleWrenchIconPaint() OVERRIDE { ++schedule_paint_count_; }

 protected:
  MessageLoopForUI message_loop_;  // Needed for ui::Animation.
  TestingProfile profile_;
  int schedule_paint_count_;
  ui::ThemeProvider* theme_provider_;
  WrenchIconPainter painter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WrenchIconPainterTest);
};

// Nothing to test here. Just exercise the paint code to verify that nothing
// leaks or crashes.
TEST_F(WrenchIconPainterTest, Paint) {
  gfx::Rect rect(0, 0, 29, 29);
  gfx::Canvas canvas(rect.size(), ui::SCALE_FACTOR_100P, true);

  painter_.Paint(&canvas, theme_provider_, rect, WrenchIconPainter::BEZEL_NONE);
  painter_.Paint(
      &canvas, theme_provider_, rect, WrenchIconPainter::BEZEL_HOVER);
  painter_.Paint(
      &canvas, theme_provider_, rect, WrenchIconPainter::BEZEL_PRESSED);

  painter_.SetSeverity(WrenchIconPainter::SEVERITY_LOW);
  painter_.Paint(
      &canvas, theme_provider_, rect, WrenchIconPainter::BEZEL_PRESSED);
  painter_.SetSeverity(WrenchIconPainter::SEVERITY_MEDIUM);
  painter_.Paint(
      &canvas, theme_provider_, rect, WrenchIconPainter::BEZEL_PRESSED);
  painter_.SetSeverity(WrenchIconPainter::SEVERITY_HIGH);
  painter_.Paint(
      &canvas, theme_provider_, rect, WrenchIconPainter::BEZEL_PRESSED);

  painter_.set_badge(*theme_provider_->GetImageSkiaNamed(IDR_PRODUCT_LOGO_16));
  painter_.Paint(
      &canvas, theme_provider_, rect, WrenchIconPainter::BEZEL_PRESSED);
}

TEST_F(WrenchIconPainterTest, PaintCallback) {
  painter_.SetSeverity(WrenchIconPainter::SEVERITY_LOW);
  schedule_paint_count_ = 0;
  painter_.AnimationProgressed(NULL);
  EXPECT_EQ(1, schedule_paint_count_);
}
