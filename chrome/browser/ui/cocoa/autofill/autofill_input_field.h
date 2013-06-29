// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_INPUT_FIELD_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_INPUT_FIELD_H_

// Protocol to allow access to any given input field in an Autofill dialog, no
// matter what the underlying control is.
@protocol AutofillInputField

@property(nonatomic, assign) BOOL invalid;
@property(nonatomic, copy) NSString* fieldValue;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_INPUT_FIELD_H_
