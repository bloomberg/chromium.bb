// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MANAGE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MANAGE_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_content_view_controller.h"

class ManagePasswordsBubbleModel;

// Shows a list of passwords saved for the current site..
@interface PasswordItemListView : NSView {
  base::scoped_nsobject<NSArray> itemViews_;
}
- (id)initWithModel:(ManagePasswordsBubbleModel*)model;
@end

@interface PasswordItemListView (Testing)
@property(readonly) NSArray* itemViews;
@end

// Informs the user that no passwords are stored for the current site.
@interface NoPasswordsView : NSTextField
- (id)initWithWidth:(CGFloat)width;
@end

// Manages the view that allows users to manage passwords for a site.
@interface ManagePasswordsBubbleManageViewController
    : ManagePasswordsBubbleContentViewController {
 @private
  ManagePasswordsBubbleModel* model_;  // weak
  id<ManagePasswordsBubbleContentViewDelegate> delegate_;  // weak
  base::scoped_nsobject<NSButton> doneButton_;
  base::scoped_nsobject<NSButton> manageButton_;
  base::scoped_nsobject<NSView> contentView_;
}
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
@end

@interface ManagePasswordsBubbleManageViewController (Testing)
@property(readonly) NSButton* doneButton;
@property(readonly) NSButton* manageButton;
@property(readonly) NSView* contentView;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MANAGE_VIEW_CONTROLLER_H_
