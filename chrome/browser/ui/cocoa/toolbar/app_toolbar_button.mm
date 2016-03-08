// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/app_toolbar_button.h"

#include "base/macros.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"

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

- (SkColor)iconColor:(BOOL)themeIsDark {
  const SkColor normalColor = SkColorSetRGB(0x5A, 0x5A, 0x5A);
  const SkColor normalIncognitoColor = SkColorSetRGB(0xFF, 0xFF, 0xFF);
  const SkColor severityMedColor = SkColorSetRGB(0xF0, 0x93, 0x00);
  const SkColor severityMedIncognitoColor = SkColorSetRGB(0xF7, 0xCB, 0x4D);
  const SkColor severityHighColor = SkColorSetRGB(0xC5, 0x39, 0x29);
  const SkColor severityHighIncognitoColor = SkColorSetRGB(0xE6, 0x73, 0x7C);

  switch (severity_) {
    case AppMenuIconPainter::Severity::SEVERITY_NONE:
    case AppMenuIconPainter::Severity::SEVERITY_LOW:
      return themeIsDark ? normalIncognitoColor : normalColor;
      break;

    case AppMenuIconPainter::Severity::SEVERITY_MEDIUM:
      return themeIsDark ? severityMedIncognitoColor : severityMedColor;
      break;

    case AppMenuIconPainter::Severity::SEVERITY_HIGH:
      return themeIsDark ? severityHighIncognitoColor : severityHighColor;
      break;

    default:
      break;
  }
}

- (void)setSeverity:(AppMenuIconPainter::Severity)severity
      shouldAnimate:(BOOL)shouldAnimate {
  if (severity != severity_) {
    severity_ = severity;
    [self setImagesFromIconId:gfx::VectorIconId::BROWSER_TOOLS];
    [self setNeedsDisplay:YES];
  }
}

@end
