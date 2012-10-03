// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FLIPPED_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_FLIPPED_VIEW_H_

#import <Cocoa/Cocoa.h>

// A view where the Y axis is flipped such that the origin is at the top left
// and Y value increases downwards. Drawing is flipped so that layout of the
// sections is easier. Apple recommends flipping the coordinate origin when
// doing a lot of text layout because it's more natural.
@interface FlippedView : NSView
@end

#endif  // CHROME_BROWSER_UI_COCOA_FLIPPED_VIEW_H_
