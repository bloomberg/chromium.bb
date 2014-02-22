// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_THROBBER_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_TABS_THROBBER_VIEW_H_

#import <Cocoa/Cocoa.h>

@protocol ThrobberDataDelegate;

// Draws an image animating down to the bottom and then another image
// animating up from the bottom. Stops once the animation is complete.

@interface ThrobberView : NSView {
 @private
  id<ThrobberDataDelegate> dataDelegate_;
}

// Creates a filmstrip view with |frame| and image |image|.
+ (id)filmstripThrobberViewWithFrame:(NSRect)frame
                               image:(NSImage*)image;

// Creates a toast view with |frame| and specified images.
+ (id)toastThrobberViewWithFrame:(NSRect)frame
                     beforeImage:(NSImage*)beforeImage
                      afterImage:(NSImage*)afterImage;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TABS_THROBBER_VIEW_H_
