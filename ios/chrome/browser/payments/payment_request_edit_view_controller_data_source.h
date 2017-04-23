// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

@class EditorField;

// Data source protocol for PaymentRequestEditViewController.
@protocol PaymentRequestEditViewControllerDataSource<NSObject>

// Returns the list of field definitions for the editor.
- (NSArray<EditorField*>*)editorFields;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_
