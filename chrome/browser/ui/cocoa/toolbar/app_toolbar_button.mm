// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/app_toolbar_button.h"

#include "base/macros.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "ui/gfx/color_palette.h"

class AppMenuButtonIconPainterDelegateMac :
    public AppMenuIconPainter::Delegate {
 public:
  explicit AppMenuButtonIconPainterDelegateMac(NSButton* button) :
      button_(button) {}
  ~AppMenuButtonIconPainterDelegateMac() override {}

  void ScheduleAppMenuIconPaint() override {
    [button_ setNeedsDisplay:YES];
  }

 private:
  NSButton* button_;

  DISALLOW_COPY_AND_ASSIGN(AppMenuButtonIconPainterDelegateMac);
};

@interface AppToolbarButton ()
- (void)commonInit;
@end

@implementation AppToolbarButton

- (instancetype)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self commonInit];
  }
  return self;
}

- (void)awakeFromNib {
  [self commonInit];
}

- (void)commonInit {
  view_id_util::SetID(self, VIEW_ID_APP_MENU);
  delegate_.reset(new AppMenuButtonIconPainterDelegateMac(self));
  severity_ = AppMenuIconPainter::Severity::SEVERITY_NONE;
}

- (gfx::VectorIconId)vectorIconId {
  return gfx::VectorIconId::BROWSER_TOOLS;
}

- (SkColor)vectorIconColor:(BOOL)themeIsDark {
  switch (severity_) {
    case AppMenuIconPainter::Severity::SEVERITY_NONE:
    case AppMenuIconPainter::Severity::SEVERITY_LOW:
      return themeIsDark ? SK_ColorWHITE : gfx::kChromeIconGrey;
      break;

    case AppMenuIconPainter::Severity::SEVERITY_MEDIUM:
      return themeIsDark ? gfx::kGoogleYellow300 : gfx::kGoogleYellow700;
      break;

    case AppMenuIconPainter::Severity::SEVERITY_HIGH:
      return themeIsDark ? gfx::kGoogleRed300 : gfx::kGoogleRed700;
      break;

    default:
      break;
  }
}

- (void)setSeverity:(AppMenuIconPainter::Severity)severity
      shouldAnimate:(BOOL)shouldAnimate {
  if (severity != severity_) {
    severity_ = severity;
    // Update the button state images with the new severity color.
    [self resetButtonStateImages];
  }
}

@end
