// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_CONSUMER_H_

#import <UIKit/UIKit.h>

// ToolsConsumer sets the current appearance of the ToolsMenu based on
// various sources provided by the mediator.
@protocol ToolsConsumer
// PLACEHOLDER: The current methods in this protocol are not intended to be
// final and might change depending on how ToolsMenuVC model is set up.
// Sets the Tools Menu items.
- (void)setToolsMenuItems:(NSArray*)menuItems;
// Sets a flag so the consumer knows if it should display the Menu overflow
// controls.
- (void)setDisplayOverflowControls:(BOOL)displayOverflowControls;
// Updates the tools menu overflow controls with the current loading state.
- (void)setIsLoading:(BOOL)isLoading;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_CONSUMER_H_
