// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

@class EditorField;
@class CollectionViewItem;

// The possible states the view controller can be in.
typedef NS_ENUM(NSInteger, EditViewControllerState) {
  // The view controller is used for creating.
  EditViewControllerStateCreate,
  // The view controller is used to editing.
  EditViewControllerStateEdit,
};

// Data source protocol for PaymentRequestEditViewController.
@protocol PaymentRequestEditViewControllerDataSource<NSObject>

// The current state of the view controller.
@property(nonatomic, assign) EditViewControllerState state;

// Returns the header item. May be nil.
- (CollectionViewItem*)headerItem;

// Returns whether the header item should hide its background.
- (BOOL)shouldHideBackgroundForHeaderItem;

// Formats the editor field value, if necessary.
- (void)formatValueForEditorField:(EditorField*)field;

// Returns an icon that identifies |field| or its current value. May be nil.
- (UIImage*)iconIdentifyingEditorField:(EditorField*)field;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_
