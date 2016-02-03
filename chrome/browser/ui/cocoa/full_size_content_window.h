// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FULL_SIZE_CONTENT_WINDOW_H_
#define CHROME_BROWSER_UI_COCOA_FULL_SIZE_CONTENT_WINDOW_H_

#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"

#include "base/mac/scoped_nsobject.h"

// By default, the contentView does not occupy the full size of a framed
// window. Chrome still wants to draw in the title bar. Historically, Chrome
// has done this by adding subviews directly to the root view. This causes
// several problems. The most egregious is related to layer ordering when the
// root view does not have a layer. By giving the contentView the same size as
// the window, there is no longer any need to add subviews to the root view.
//
// If the window does not have a titlebar, then its contentView already has the
// same size as the window. In this case, this class has no effect.
//
// This class currently does not support changing the window's style after the
// window has been initialized.
@interface FullSizeContentWindow : ChromeEventProcessingWindow {
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

// Forces |chromeWindowView_| to resize to the given size. This need to be
// forced because by default, the contentView will always have the same size
// as the window. If |chromeWindowView_| is empty, we will set the frame of
// [window contentView] to the given frame.
- (void)forceContentViewFrame:(NSRect)frame;

@end

#endif  // CHROME_BROWSER_UI_COCOA_FULL_SIZE_CONTENT_WINDOW_H_
