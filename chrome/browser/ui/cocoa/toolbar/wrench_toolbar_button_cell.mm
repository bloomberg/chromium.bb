// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/wrench_toolbar_button_cell.h"

#import "chrome/browser/ui/cocoa/themed_window.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/geometry/rect.h"

class AppMenuIconPainterDelegateMac : public AppMenuIconPainter::Delegate {
 public:
  explicit AppMenuIconPainterDelegateMac(NSCell* cell) : cell_(cell) {}
  ~AppMenuIconPainterDelegateMac() override {}

  void ScheduleAppMenuIconPaint() override {
    [[cell_ controlView] setNeedsDisplay:YES];
  }

 private:
  NSCell* cell_;

  DISALLOW_COPY_AND_ASSIGN(AppMenuIconPainterDelegateMac);
};

@interface WrenchToolbarButtonCell ()
- (void)commonInit;
- (AppMenuIconPainter::BezelType)currentBezelType;
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
    iconPainter_->Paint(&canvas, [[controlView window] themeProvider],
                        gfx::Rect(NSRectToCGRect(cellFrame)),
                        [self currentBezelType]);
  }
  canvas.Restore();

  [self drawFocusRingWithFrame:cellFrame inView:controlView];
}

- (void)setSeverity:(AppMenuIconPainter::Severity)severity
      shouldAnimate:(BOOL)shouldAnimate {
  iconPainter_->SetSeverity(severity, shouldAnimate);
}

- (void)commonInit {
  delegate_.reset(new AppMenuIconPainterDelegateMac(self));
  iconPainter_.reset(new AppMenuIconPainter(delegate_.get()));
}

- (AppMenuIconPainter::BezelType)currentBezelType {
  if ([self isHighlighted])
    return AppMenuIconPainter::BEZEL_PRESSED;
  if ([self isMouseInside])
    return AppMenuIconPainter::BEZEL_HOVER;
  return AppMenuIconPainter::BEZEL_NONE;
}

@end
