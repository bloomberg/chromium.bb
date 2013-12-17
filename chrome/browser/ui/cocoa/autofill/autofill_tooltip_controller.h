// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_TOOLTIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_TOOLTIP_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

@class AutofillBubbleController;
@class AutofillTooltip;

@protocol AutofillTooltipDelegate
- (void)didBeginHover;
- (void)didEndHover;
@end

// Controller for the Tooltip view, which handles displaying/hiding the
// tooltip bubble on hover.
@interface AutofillTooltipController
    : NSViewController<AutofillTooltipDelegate> {
 @private
  base::scoped_nsobject<AutofillTooltip> view_;
  AutofillBubbleController* bubbleController_;
  NSString* message_;
}

// |message| to display in the tooltip.
@property(copy, nonatomic) NSString* message;

- (id)init;
- (void)setImage:(NSImage*)image;

@end;

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_TOOLTIP_CONTROLLER_H_
