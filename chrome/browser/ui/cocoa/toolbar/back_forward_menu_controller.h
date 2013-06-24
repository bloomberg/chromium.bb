// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOOLBAR_BACK_FORWARD_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TOOLBAR_BACK_FORWARD_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"

@class MenuButton;

typedef BackForwardMenuModel::ModelType BackForwardMenuType;
const BackForwardMenuType BACK_FORWARD_MENU_TYPE_BACK =
    BackForwardMenuModel::BACKWARD_MENU;
const BackForwardMenuType BACK_FORWARD_MENU_TYPE_FORWARD =
    BackForwardMenuModel::FORWARD_MENU;

// A class that manages the back/forward menu (and delayed-menu button, and
// model).

@interface BackForwardMenuController : NSObject<NSMenuDelegate> {
 @private
  BackForwardMenuType type_;
  MenuButton* button_;  // Weak; comes from nib.
  scoped_ptr<BackForwardMenuModel> model_;
  base::scoped_nsobject<NSMenu> backForwardMenu_;
}

// Type (back or forwards); can only be set on initialization.
@property(readonly, nonatomic) BackForwardMenuType type;

- (id)initWithBrowser:(Browser*)browser
            modelType:(BackForwardMenuType)type
               button:(MenuButton*)button;

@end  // @interface BackForwardMenuController

#endif  // CHROME_BROWSER_UI_COCOA_TOOLBAR_BACK_FORWARD_MENU_CONTROLLER_H_
