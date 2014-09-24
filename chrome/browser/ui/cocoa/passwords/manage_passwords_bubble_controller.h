// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_content_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_never_save_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_pending_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

// Controller for the Cocoa manage passwords bubble. Transitions through several
// views according to user interaction and updates the password management state
// accordingly.
@interface ManagePasswordsBubbleController
    : BaseBubbleController<ManagePasswordsBubbleContentViewDelegate,
                           ManagePasswordsBubbleNeverSaveViewDelegate,
                           ManagePasswordsBubblePendingViewDelegate> {
 @private
  ManagePasswordsBubbleModel* model_;
  base::scoped_nsobject<ManagePasswordsBubbleContentViewController>
      currentController_;
}
- (id)initWithParentWindow:(NSWindow*)parentWindow
                     model:(ManagePasswordsBubbleModel*)model;
@end

@interface ManagePasswordsBubbleController (Testing)
@property(readonly) NSViewController* currentController;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_CONTROLLER_H_
