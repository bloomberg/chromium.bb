// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BACK_FORWARD_MENU_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BACK_FORWARD_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/back_forward_menu_model.h"

@class DelayedMenuButton;

typedef BackForwardMenuModel::ModelType BackForwardMenuType;
const BackForwardMenuType BACK_FORWARD_MENU_TYPE_BACK =
    BackForwardMenuModel::BACKWARD_MENU;
const BackForwardMenuType BACK_FORWARD_MENU_TYPE_FORWARD =
    BackForwardMenuModel::FORWARD_MENU;

// A class that manages the back/forward menu (and delayed-menu button, and
// model).

@interface BackForwardMenuController : NSObject {
 @private
  BackForwardMenuType type_;
  DelayedMenuButton* button_;           // Weak; comes from nib.
  scoped_ptr<BackForwardMenuModel> model_;
  scoped_nsobject<NSMenu> backForwardMenu_;
}

// Type (back or forwards); can only be set on initialization.
@property(readonly, nonatomic) BackForwardMenuType type;

- (id)initWithBrowser:(Browser*)browser
            modelType:(BackForwardMenuType)type
               button:(DelayedMenuButton*)button;

@end  // @interface BackForwardMenuController

#endif  // CHROME_BROWSER_COCOA_BACK_FORWARD_MENU_CONTROLLER_H_
