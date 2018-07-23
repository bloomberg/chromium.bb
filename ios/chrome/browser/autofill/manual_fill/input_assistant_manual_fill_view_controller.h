// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_INPUT_ASSISTANT_MANUAL_FILL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_INPUT_ASSISTANT_MANUAL_FILL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/manual_fill/manual_fill_view_controller.h"

// This class allows the user to manual fill data by adding input assistant
// items  to the first responder. Which then uses pop overs to show the
// available options. Currently the `inputAssistantItem` property is only
// available on iPads.
@interface InputAssistantManualFillViewController : ManualFillViewController
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_INPUT_ASSISTANT_MANUAL_FILL_VIEW_CONTROLLER_H_
