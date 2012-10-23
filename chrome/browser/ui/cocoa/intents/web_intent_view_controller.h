// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_VIEW_CONTROLLER_H_

#include <Cocoa/Cocoa.h>

// Abstract base class for all web intent view controllers.
@interface WebIntentViewController : NSViewController

// The minimum inner frame for a web intent view. This does not include the
// space for padding around the edge of the view.
+ (NSRect)minimumInnerFrame;

// Resizes the view to fit its contents and lays out subviews.
// The default implementation resizes the view to fit
// |-minimumSizeForInnerWidth:| and calls |-layoutSubviewsWithinFrame:| to
// perform layout. Either this method must be overridden or the below two.
- (void)sizeToFitAndLayout;

// Gets the size of the view for the given width. |innerWidth| and the returned
// size does not include space for padding around the edge of the view.
// This method must be overridden if |-sizeToFitAndLayout| is not.
- (NSSize)minimumSizeForInnerWidth:(CGFloat)innerWidth;

// Layout subviews in the given frame.
// This method must be overridden if |-sizeToFitAndLayout| is not.
- (void)layoutSubviewsWithinFrame:(NSRect)innerFrame;

// Called after the view has been removed from the super view. Subclasses can
// override this to clear and state.
- (void)viewRemovedFromSuperview;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_VIEW_CONTROLLER_H_
