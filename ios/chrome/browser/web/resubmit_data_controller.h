// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_RESUBMIT_DATA_CONTROLLER_H_
#define IOS_CHROME_BROWSER_WEB_RESUBMIT_DATA_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Handles the action sheet that is presenting to confirm POST data
// resubmission.
@interface ResubmitDataController : NSObject

- (instancetype)initWithContinueBlock:(ProceduralBlock)continueBlock
                          cancelBlock:(ProceduralBlock)cancelBlock
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Presents the action sheet. On regular horizontal size class, it is presented
// in a popover from |rect| in |view|.
- (void)presentActionSheetFromRect:(CGRect)rect inView:(UIView*)view;

// Dismisses the action sheet.
- (void)dismissActionSheet;

@end

#endif  // IOS_CHROME_BROWSER_WEB_RESUBMIT_DATA_CONTROLLER_H_
