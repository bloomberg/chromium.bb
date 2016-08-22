// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_LIST_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_LIST_VIEW_CONTROLLER_H_

#include <vector>

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

class ManagePasswordsBubbleModel;

typedef std::vector<autofill::PasswordForm> PasswordFormsVector;

// Handles callbacks from ManagePasswordItemViewController.
@protocol PasswordItemDelegate<NSObject>
// Retrieves the model
@property(readonly, nonatomic) ManagePasswordsBubbleModel* model;

@property(readonly, nonatomic) CGFloat firstColumnMaxWidth;
@property(readonly, nonatomic) CGFloat secondColumnMaxWidth;
@end

// Shows a list of usernames/passwords.
@interface PasswordsListViewController
    : NSViewController<PasswordItemDelegate> {
 @private
  ManagePasswordsBubbleModel* model_;  // weak
  // Array of ManagePasswordItemViewController for each row.
  base::scoped_nsobject<NSArray> itemViews_;
  // Maximum widthes of columns across all the rows.
  CGFloat firstColumnMaxWidth_;
  CGFloat secondColumnMaxWidth_;
}
- (id)initWithModelAndForms:(ManagePasswordsBubbleModel*)model
                      forms:(const PasswordFormsVector*)password_forms;
- (id)initWithModelAndForm:(ManagePasswordsBubbleModel*)model
                      form:(const autofill::PasswordForm*)form;
@end

@interface PasswordsListViewController (Testing)
@property(readonly) NSArray* itemViews;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORDS_LIST_VIEW_CONTROLLER_H_
