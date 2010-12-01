// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHEVRON_MENU_BUTTON_CELL_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHEVRON_MENU_BUTTON_CELL_H_
#pragma once

#import "chrome/browser/ui/cocoa/clickhold_button_cell.h"

@interface ChevronMenuButtonCell : ClickHoldButtonCell {
}

// Adds a gradient border to the RHS of the cell when not hovered.
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHEVRON_MENU_BUTTON_CELL_H_
