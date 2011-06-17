// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_TIP_DRAWING_MODEL_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_TIP_DRAWING_MODEL_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"

@class InfoBarContainerController;
@class InfoBarController;
@class InfoBarGradientView;

namespace infobars {

// The base height of an infobar, in which all the content fits. This is the
// same value as set in the XIB. The tip height will be added to this value.
const CGFloat kBaseHeight = 36.0;

// For the 0th infobar with the bookmark bar attached, this is the maximum
// height of the anti-spoof tip.
const CGFloat kExpandedTipHeight = 24.0;

// For all other infobars, this is the maximum height of the anti-spoof tip.
const CGFloat kDefaultTipHeight = 12.0;

// The width of the tip of the 0th infobar with the bookmark bar attached.
const CGFloat kExpandedTipWidth = 29.0;

// The width of the tip for all other infobars.
const CGFloat kDefaultTipWidth = 23.0;

// This structure contains the necessary information that the
// InfobarGradientView uses to draw the background and anti-spoof tip.
struct Tip {
  Tip();
  ~Tip();

  // The bezier path for the tip itself.
  scoped_nsobject<NSBezierPath> path;
  // The starting point for the above |path|. The path around infobar's frame
  // should |-lineToPoint:| before appending the tip |path|.
  NSPoint point;
  // The middle point of the path, at the maximum tip_height.
  NSPoint mid_point;
  // The height of the tip, which is used when the Model is constructing |path|.
  CGFloat tip_height;
};

}  // namespace infobars

// This class helps infobars draw. It provides the height information for each
// and vends the above |Tip| structure when requested. This is owned by the
// container controller (and thus there's one per BrowserWindow), and each
// InfobarGradientView maintains a weak reference to this as well.
@interface InfobarTipDrawingModel : NSObject {
 @private
  InfoBarContainerController* containerController_;  // weak

  // The current infobars::Tip object as it's being constructed.
  infobars::Tip* tip_;  // weak
}

- (id)initWithContainerController:(InfoBarContainerController*)controller;

// Creates a new infobar::Tip for a given controller's view. Caller is
// responsible for deleting the result. This can return NULL if the controller
// is in the process of animating closed.
- (infobars::Tip*)createTipForView:(InfoBarGradientView*)view;

// Returns the total height of an infobar (including the tip) for a given
// controller.
- (CGFloat)totalHeightForController:(InfoBarController*)controller;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_TIP_DRAWING_MODEL_H_
