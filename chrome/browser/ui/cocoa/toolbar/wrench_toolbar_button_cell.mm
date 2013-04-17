// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/wrench_toolbar_button_cell.h"

#import "chrome/browser/ui/cocoa/themed_window.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/rect.h"

class WrenchIconPainterDelegateMac : public WrenchIconPainter::Delegate {
 public:
  explicit WrenchIconPainterDelegateMac(NSCell* cell) : cell_(cell) {}
  virtual ~WrenchIconPainterDelegateMac() {}

  virtual void ScheduleWrenchIconPaint() OVERRIDE {
    [[cell_ controlView] setNeedsDisplay:YES];
  }

 private:
  NSCell* cell_;

  DISALLOW_COPY_AND_ASSIGN(WrenchIconPainterDelegateMac);
};

@interface WrenchToolbarButtonCell ()
- (void)commonInit;
- (WrenchIconPainter::BezelType)currentBezelType;
@end

@implementation WrenchToolbarButtonCell

- (id)initTextCell:(NSString*)text {
  if ((self = [super initTextCell:text])) {
    [self commonInit];
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    [self commonInit];
  }
  return self;
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  gfx::CanvasSkiaPaint canvas(cellFrame, false);
  canvas.set_composite_alpha(true);
  canvas.SaveLayerAlpha(255 *
                        [self imageAlphaForWindowState:[controlView window]]);
  ui::ThemeProvider* themeProvider = [[controlView window] themeProvider];
  if (themeProvider) {
    wrenchIconPainter_->Paint(&canvas,
                              [[controlView window] themeProvider],
                              gfx::Rect(NSRectToCGRect(cellFrame)),
                              [self currentBezelType]);
  }
  canvas.Restore();

  [self drawFocusRingWithFrame:cellFrame inView:controlView];
}

- (void)setSeverity:(WrenchIconPainter::Severity)severity
      shouldAnimate:(BOOL)shouldAnimate {
  wrenchIconPainter_->SetSeverity(severity, shouldAnimate);
}

- (void)commonInit {
  delegate_.reset(new WrenchIconPainterDelegateMac(self));
  wrenchIconPainter_.reset(new WrenchIconPainter(delegate_.get()));
}

- (WrenchIconPainter::BezelType)currentBezelType {
  if ([self isHighlighted])
    return WrenchIconPainter::BEZEL_PRESSED;
  if ([self isMouseInside])
    return WrenchIconPainter::BEZEL_HOVER;
  return WrenchIconPainter::BEZEL_NONE;
}

@end
