// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_DOWN_ARROW_POPUP_MENU_CELL_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_DOWN_ARROW_POPUP_MENU_CELL_H_

#import "chrome/browser/ui/cocoa/clickhold_button_cell.h"

// Button cell for a popup menu that shows a static title as well as a down
// arrow. Exposes the same public API as the ClickHoldButtonCell, but
// has modified rendering to add static title.
@interface DownArrowPopupMenuCell : ClickHoldButtonCell
@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_DOWN_ARROW_POPUP_MENU_CELL_H_
