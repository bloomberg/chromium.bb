// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_LAYOUT_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_LAYOUT_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"

class SimpleGridLayout;

// A view that carries its own layout manager. Re-layouts on setFrame:.
@interface LayoutView : NSView {
 @private
  scoped_ptr<SimpleGridLayout> layout_;
}

// Sets a layout manager and takes ownership of it.
- (void)setLayoutManager:(scoped_ptr<SimpleGridLayout>)layout;

// Return a pointer to layout manager, still owned by the view.
- (SimpleGridLayout*)layoutManager;

// Re-layout subviews according to layout manager.
- (void)performLayout;

// Query the layout manager for the best height for specified |width|.
- (CGFloat)preferredHeightForWidth:(CGFloat)width;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_LAYOUT_VIEW_H_
