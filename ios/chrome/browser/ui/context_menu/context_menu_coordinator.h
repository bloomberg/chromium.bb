// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

namespace web {
struct ContextMenuParams;
}

// Abstracts displaying context menus for all device form factors. Will show a
// sheet on the phone and use a popover on a tablet.
@interface ContextMenuCoordinator : NSObject

// Whether the context menu is visible. This will be true after |-start| is
// called until a subsequent |-stop|.
@property(nonatomic, readonly, getter=isVisible) BOOL visible;

// Initializes with details provided in |params|. Context menu will be presented
// from |viewController|.
- (instancetype)initWithViewController:(UIViewController*)viewController
                                params:(const web::ContextMenuParams&)params;

// Adds an item at the end of the menu if |visible| is false.
- (void)addItemWithTitle:(NSString*)title action:(ProceduralBlock)action;

// Displays the context menu.
- (void)start;
// Dismisses the context menu. Any menu items which have been added will be
// cleared after this call.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_COORDINATOR_H_
