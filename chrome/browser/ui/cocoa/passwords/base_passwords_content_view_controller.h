// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_BASE_PASSWORDS_CONTENT_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_BASE_PASSWORDS_CONTENT_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

class ManagePasswordsBubbleModel;

// Handles user interaction with the content view.
@protocol ManagePasswordsBubbleContentViewDelegate<NSObject>

// The user performed an action that should dismiss the bubble.
- (void)viewShouldDismiss;

// Returns the model object.
@property(nonatomic, readonly) ManagePasswordsBubbleModel* model;

@end

// Base class for a state of the password management bubble.
@interface ManagePasswordsBubbleContentViewController : NSViewController {
  id<ManagePasswordsBubbleContentViewDelegate> delegate_;  // Weak.
}
- (id)initWithDelegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
- (NSButton*)addButton:(NSString*)title
                toView:(NSView*)view
                target:(id)target
                action:(SEL)action;
- (NSTextField*)addTitleLabel:(NSString*)title toView:(NSView*)view;
- (NSTextField*)addLabel:(NSString*)title toView:(NSView*)view;
- (void)bubbleWillDisappear;

// Returns the default button for the bubble.
- (NSButton*)defaultButton;

@property(nonatomic, assign)
    id<ManagePasswordsBubbleContentViewDelegate> delegate;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_BASE_PASSWORDS_CONTENT_VIEW_CONTROLLER_H_
