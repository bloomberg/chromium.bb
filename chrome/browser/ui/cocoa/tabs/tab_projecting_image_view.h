// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_TAB_PROJECTING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_TABS_TAB_PROJECTING_IMAGE_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/cocoa/tabs/throbbing_image_view.h"

// ImageView for when a tab is in "projecting" mode. The view is made up of
// three images: background image (original favicon), projector sheet and an
// animated glow. This view paints outside the favicon bounds due to the glow.
@interface TabProjectingImageView : ThrobbingImageView {
 @private
  scoped_nsobject<NSImage> projectorImage_;
}

- (id)initWithFrame:(NSRect)rect
       backgroundImage:(NSImage*)backgroundImage
        projectorImage:(NSImage*)projectorImage
            throbImage:(NSImage*)throbImage
            durationMS:(int)durationMS
    animationContainer:(ui::AnimationContainer*)animationContainer;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TABS_TAB_PROJECTING_IMAGE_VIEW_H_
