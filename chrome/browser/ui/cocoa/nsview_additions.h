// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NSVIEW_ADDITIONS_H_
#define CHROME_BROWSER_UI_COCOA_NSVIEW_ADDITIONS_H_
#pragma once

#import <Cocoa/Cocoa.h>

@interface NSView (ChromeAdditions)

// Returns the line width that will generate a 1 pixel wide line.
- (CGFloat)cr_lineWidth;

@end

#endif  // CHROME_BROWSER_UI_COCOA_NSVIEW_ADDITIONS_H_
