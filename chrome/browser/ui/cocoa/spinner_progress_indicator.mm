// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/spinner_progress_indicator.h"

#include "base/timer.h"
#include "chrome/browser/download/download_util.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/rect.h"

namespace {

// Interval between spinner updates in miliseconds.
const int kTimerIntervalMs = 1000 / 30;

// Degrees to rotate the indeterminte spinner per second.
const CGFloat kSpinRateDegreesPerSecond = 270;

// Callback for indeterminte spinner animation timer.
void OnTimer(SpinnerProgressIndicator* indicator) {
  [indicator setNeedsDisplay:YES];
}

}  // namespace

@interface SpinnerProgressIndicator ()
- (int)progressAngle;
- (void)updateTimer;
@end

@implementation SpinnerProgressIndicator

@synthesize percentDone = percentDone_;
@synthesize isIndeterminate = isIndeterminate_;

- (void)setPercentDone:(int)percent {
  percentDone_ = percent;
  [self setNeedsDisplay:YES];
  [self updateTimer];
}

- (void)setIsIndeterminate:(BOOL)isIndeterminate {
  isIndeterminate_ = isIndeterminate;
  [self setNeedsDisplay:YES];
  [self updateTimer];
}

- (void)sizeToFit {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* foreground = rb.GetImageSkiaNamed(
      IDR_WEB_INTENT_PROGRESS_FOREGROUND);
  NSRect frame = [self frame];
  frame.size.width = foreground->width();
  frame.size.height = foreground->height();
  [self setFrame:frame];
}

- (void)drawRect:(NSRect)rect {
  NSRect bounds = [self bounds];
  gfx::CanvasSkiaPaint canvas(bounds, false);
  canvas.set_composite_alpha(true);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* background = rb.GetImageSkiaNamed(
      IDR_WEB_INTENT_PROGRESS_BACKGROUND);
  gfx::ImageSkia* foreground = rb.GetImageSkiaNamed(
      IDR_WEB_INTENT_PROGRESS_FOREGROUND);

  download_util::PaintCustomDownloadProgress(
      &canvas,
      *background,
      *foreground,
      foreground->width(),
      gfx::Rect(NSRectToCGRect(bounds)),
      [self progressAngle],
      isIndeterminate_ ? -1 : percentDone_);
}

- (void)viewDidMoveToWindow {
  [self updateTimer];
}

- (int)progressAngle {
  if (!isIndeterminate_)
    return download_util::kStartAngleDegrees;
  base::TimeDelta delta = base::TimeTicks::Now() - startTime_;
  int angle = delta.InSecondsF() * kSpinRateDegreesPerSecond;
  return angle % 360;
}

- (void)updateTimer {
  // Only run the timer if the control is in a window and showing an
  // indeterminte progress.
  if (![self window] || !isIndeterminate_) {
    timer_.reset();
    return;
  }

  if (!timer_.get()) {
    startTime_ = base::TimeTicks::Now();
    timer_.reset(new base::Timer(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kTimerIntervalMs),
        base::Bind(OnTimer, self),
        true));
    timer_->Reset();
  }
}

@end
