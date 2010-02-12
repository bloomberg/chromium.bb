// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_VERTICAL_LAYOUT_VIEW_
#define CHROME_BROWSER_COCOA_VERTICAL_LAYOUT_VIEW_

#import <Cocoa/Cocoa.h>

// A view class that automatically performs layout of child views based
// on paint order of the children in the view hierarchy.  The children are
// arranged top-to-bottom (in y-order) based on each child's height.
// Horizontal (x) positions are left as specified.  Layout is performed when
// children are added, removed, or have their frames changed.  Layout is also
// performed when this view (|self|) has its frame changed.
// Autoresizing is disabled for |VerticalLayoutView|s.
@interface VerticalLayoutView : NSView {
}

@end

#endif // CHROME_BROWSER_COCOA_VERTICAL_LAYOUT_VIEW_
