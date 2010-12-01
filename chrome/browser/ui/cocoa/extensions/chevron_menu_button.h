// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHEVRON_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHEVRON_MENU_BUTTON_H_
#pragma once

#import "chrome/browser/ui/cocoa/menu_button.h"

@interface ChevronMenuButton : MenuButton {
}

// Overrides cell class with |ChevronMenuButtonCell|.
+ (Class)cellClass;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_CHEVRON_MENU_BUTTON_H_
