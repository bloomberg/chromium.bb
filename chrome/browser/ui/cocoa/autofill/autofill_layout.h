// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_LAYOUT_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_LAYOUT_H_

#import <Cocoa/Cocoa.h>

// Defines a protocol that allows resizing a view hierarchy based on the size
// requirements of the subviews. Is implemented by either views or view
// controllers.
// The way this works together is:
// * Subview indicates by calling -requestRelayout on the window controller.
// * Window controller queries subviews for preferredSize to determine the
//   total size of the contentView, adjusts subview origins appropriately,
//   and calls performLayout on each subview.
// * Subviews then recursively do the same thing.
@protocol AutofillLayout

// Query the preferred size, without actually layouting.
// Akin to -intrinsicContentSize on 10.7
- (NSSize)preferredSize;

// Layout the content according to the preferred size. Will not touch
// frameOrigin. If all objects in the hierarchy were custom views (and not
// view controllers), this could be replaced by overriding
// -resizeSubviewsWithOldSize:.
- (void)performLayout;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_LAYOUT_H_
