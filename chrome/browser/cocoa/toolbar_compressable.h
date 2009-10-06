// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TOOLBAR_COMPRESSABLE_H_
#define CHROME_BROWSER_COCOA_TOOLBAR_COMPRESSABLE_H_

#include "chrome/browser/tabs/tab_strip_model.h"

#import <Cocoa/Cocoa.h>

// Defines a protocol that allows one view to tell another view that it should
// remove pixels from the bottom of the view. Only ToolbarController implements
// this, but it's a protocol for unit testing reasons.
@protocol ToolbarCompressable
- (void)setShouldBeCompressed:(BOOL)compressed;
@end

#endif  // CHROME_BROWSER_COCOA_TOOLBAR_COMPRESSABLE_H_
