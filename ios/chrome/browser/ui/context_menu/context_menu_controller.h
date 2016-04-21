// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

@class ContextMenuHolder;

// Abstracts displaying context menus for all device form factors, given a
// ContextMenuHolder with the title and action to associate to each menu
// item. Will show a sheet on the phone and use a popover on a tablet.
@interface ContextMenuController : NSObject

// Whether the context menu is visible.
@property(nonatomic, readonly, getter=isVisible) BOOL visible;

// Displays a context menu. If on a tablet, |localPoint| is the point in
// |view|'s coordinates to show the popup. If a phone, |localPoint| is unused
// since the display is a sheet, but |view| is still used to attach the sheet to
// the given view.
// The |menuHolder| that will be put in the menu.
- (void)showWithHolder:(ContextMenuHolder*)menuHolder
               atPoint:(CGPoint)localPoint
                inView:(UIView*)view;

// Dismisses displayed context menu.
- (void)dismissAnimated:(BOOL)animated
      completionHandler:(ProceduralBlock)completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONTROLLER_H_
