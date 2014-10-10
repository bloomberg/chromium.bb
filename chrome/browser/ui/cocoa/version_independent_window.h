// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_VERSION_INDEPENDENT_WINDOW_H_
#define CHROME_BROWSER_UI_COCOA_VERSION_INDEPENDENT_WINDOW_H_

#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"

#include "base/mac/scoped_nsobject.h"

@interface NSWindow (VersionIndependentWindow)

// Returns the NSView closest to the root of the NSView hierarchy that is
// eligible for adding subviews.
// The frame of the view in screen coordinates is coincident with the window's
// frame in screen coordinates.
- (NSView*)cr_windowView;

@end

// By default, the contentView does not occupy the full size of a framed
// window. Chrome still wants to draw in the title bar. Historically, Chrome
// has done this by adding subviews directly to the root view. This causes
// several problems. The most egregious is related to layer ordering when the
// root view does not have a layer. By giving the contentView the same size as
// the window, there is no longer any need to add subviews to the root view.
//
// No views should be manually added to [[window contentView] superview].
// Instead, they should be added to [window cr_windowView].
//
// If the window does not have a titlebar, then its contentView already has the
// same size as the window. In this case, this class has no effect.
//
// This class currently does not support changing the window's style after the
// window has been initialized.
@interface VersionIndependentWindow : ChromeEventProcessingWindow {
 @private
  // Holds the view that replaces [window contentView]. This view has the same
  // size as the window.  Empty if there is no titlebar.
  base::scoped_nsobject<NSView> chromeWindowView_;
}

// Designated initializer.
- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation
             wantsViewsOverTitlebar:(BOOL)wantsViewsOverTitlebar;

@end

#endif  // CHROME_BROWSER_UI_COCOA_VERSION_INDEPENDENT_WINDOW_H_
