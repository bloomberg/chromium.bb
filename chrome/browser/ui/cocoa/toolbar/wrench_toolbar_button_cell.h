// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOOLBAR_WRENCH_TOOLBAR_BUTTON_CELL_H_
#define CHROME_BROWSER_UI_COCOA_TOOLBAR_WRENCH_TOOLBAR_BUTTON_CELL_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/clickhold_button_cell.h"
#include "chrome/browser/ui/toolbar/wrench_icon_painter.h"

class WrenchIconPainterDelegateMac;

// Cell for the wrench toolbar button. This is used to draw the wrench icon
// and paint severity levels.
@interface WrenchToolbarButtonCell : ClickHoldButtonCell {
 @private
  scoped_ptr<WrenchIconPainter> wrenchIconPainter_;
  scoped_ptr<WrenchIconPainterDelegateMac> delegate_;
}

- (void)setSeverity:(WrenchIconPainter::Severity)severity
      shouldAnimate:(BOOL)shouldAnimate;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TOOLBAR_WRENCH_TOOLBAR_BUTTON_CELL_H_
