// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_SELECTOR_VIEW_CONTROLLER_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_SELECTOR_VIEW_CONTROLLER_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

@class CollectionViewItem;

// The possible states the view controller can be in.
typedef NS_ENUM(NSUInteger, PaymentRequestSelectorState) {
  // The view controller is in normal state.
  PaymentRequestSelectorStateNormal,
  // The view controller is in pending state.
  PaymentRequestSelectorStatePending,
  // The view controller is in error state.
  PaymentRequestSelectorStateError,
};

// The data source for the PaymentRequestSelectorViewController. The data
// source provides the UI models for the PaymentRequestSelectorViewController
// to display and keeps track of the selected UI model, if any.
@protocol PaymentRequestSelectorViewControllerDataSource<NSObject>

// The current state of the view controller.
@property(nonatomic, readonly) PaymentRequestSelectorState state;

// Index for the currently selected item or NSUIntegerMax if there is none.
@property(nonatomic, readonly) NSUInteger selectedItemIndex;

// The header item to display in the collection, if any.
- (CollectionViewItem*)headerItem;

// The selectable items to display in the collection.
- (NSArray<CollectionViewItem*>*)selectableItems;

// The selectable item at |index| in the collection. |index| should be smaller
// than self.selectableItems.count.
- (CollectionViewItem*)selectableItemAtIndex:(NSUInteger)index;

// The "Add" button item, if any.
- (CollectionViewItem*)addButtonItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_SELECTOR_VIEW_CONTROLLER_DATA_SOURCE_H_
