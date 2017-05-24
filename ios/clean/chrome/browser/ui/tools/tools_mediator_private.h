// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MEDIATOR_PRIVATE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MEDIATOR_PRIVATE_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/ui/tools/tools_mediator.h"

// Private methods for unit testing.
@interface ToolsMediator (Testing)
- (NSArray*)menuItemsArray;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MEDIATOR_PRIVATE_H_
