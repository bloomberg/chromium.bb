// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILE_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_PROFILE_MENU_BUTTON_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"

class ProfileMenuModel;

// PopUp button that shows the multiprofile menu.
@interface ProfileMenuButton : MenuButton {
 @private
  BOOL shouldShowProfileDisplayName_;
  scoped_nsobject<NSTextFieldCell> textFieldCell_;

  scoped_nsobject<NSImage> cachedTabImage_;
  // Cache the various button states when creating |cachedTabImage_|. If
  // any of these states change then the cached image is invalidated.
  BOOL cachedTabImageIsPressed_;

  // The popup menu and its model.
  scoped_nsobject<MenuController> menu_;
  scoped_ptr<ProfileMenuModel> profile_menu_model_;
}

@property(assign,nonatomic) BOOL shouldShowProfileDisplayName;
@property(assign,nonatomic) NSString* profileDisplayName;

// Gets the size of the control that would display all its contents.
- (NSSize)desiredControlSize;
// Gets the minimum size that the control should be resized to.
- (NSSize)minControlSize;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILE_MENU_BUTTON_H_
