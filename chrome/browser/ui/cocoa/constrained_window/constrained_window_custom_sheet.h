// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_CUSTOM_SHEET_H_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_CUSTOM_SHEET_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet.h"

// Represents a custom sheet. The sheet's window is shown without using the
// system |beginSheet:...| API.
@interface CustomConstrainedWindowSheet : NSObject<ConstrainedWindowSheet> {
 @private
  base::scoped_nsobject<NSWindow> customWindow_;
}

- (id)initWithCustomWindow:(NSWindow*)customWindow;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_CUSTOM_SHEET_H_
