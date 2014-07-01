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

// In OSX 10.10, adding subviews to the root view for the NSView hierarchy
// produces warnings. To eliminate the warnings, we resize the contentView to
// fill the window, and add subviews to that.  When this class is used on OSX
// 10.9 and lower, subviews are added directly to the root view, and the
// contentView is not resized.
// http://crbug.com/380412
//
// For code to be 10.9 and 10.10 compatible, views should be added to [window
// cr_windowView] instead of [[window contentView] superview].
//
// If the window does not have a titlebar, then its contentView already has the
// same size as the window. In this case, this class has no effect.
//
// This class currently does not support changing the window's style after the
// window has been initialized.
//
// Since the contentView's size varies between OSes, several NSWindow methods
// are no longer well defined.
// - setContentSize:
// - setContentMinSize:
// - setContentMaxSize:
// - setContentAspectRatio:
// The implementation of this class on OSX 10.10 uses a hacked subclass of
// NSView. It currently does not support the above 4 methods.
@interface VersionIndependentWindow : ChromeEventProcessingWindow {
 @private
  // Holds the view that replaces [window contentView]. This view's size is the
  // same as the window's size.
  // Empty on 10.9 and lower, or if there is no titlebar.
  base::scoped_nsobject<NSView> chromeWindowView_;
}

// Designated initializer.
- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation;

@end

#endif  // CHROME_BROWSER_UI_COCOA_VERSION_INDEPENDENT_WINDOW_H_
