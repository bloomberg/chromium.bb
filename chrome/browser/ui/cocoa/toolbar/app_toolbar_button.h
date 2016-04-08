// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOOLBAR_APP_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_TOOLBAR_APP_TOOLBAR_BUTTON_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#import "chrome/browser/ui/cocoa/menu_button.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_painter.h"

class AppMenuButtonIconPainterDelegateMac;

// Button for the app toolbar button.
@interface AppToolbarButton : MenuButton {
 @private
  std::unique_ptr<AppMenuButtonIconPainterDelegateMac> delegate_;
  AppMenuIconPainter::Severity severity_;
}

- (void)setSeverity:(AppMenuIconPainter::Severity)severity
      shouldAnimate:(BOOL)shouldAnimate;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TOOLBAR_APP_TOOLBAR_BUTTON_H_
