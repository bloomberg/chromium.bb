// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_CONTROLLER_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVAutofillController;
@class CWVAutofillFormSuggestion;

CWV_EXPORT
// Protocol to receive callbacks related to autofill.
// |fieldName| is the 'name' attribute of a html field.
// |formName| is the 'name' attribute of a html <form>.
// |value| is the 'value' attribute of the html field.
// Example:
// <form name='_formName_'>
//   <input name='_fieldName_' value='_value_'>
// </form>
@protocol CWVAutofillControllerDelegate<NSObject>

@optional

// Called when a form field element receives a "focus" event.
- (void)autofillController:(CWVAutofillController*)autofillController
    didFocusOnFieldWithName:(NSString*)fieldName
                   formName:(NSString*)formName
                      value:(NSString*)value;

// Called when a form field element receives an "input" event.
- (void)autofillController:(CWVAutofillController*)autofillController
    didInputInFieldWithName:(NSString*)fieldName
                   formName:(NSString*)formName
                      value:(NSString*)value;

// Called when a form field element receives a "blur" (un-focused) event.
- (void)autofillController:(CWVAutofillController*)autofillController
    didBlurOnFieldWithName:(NSString*)fieldName
                  formName:(NSString*)formName
                     value:(NSString*)value;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_CONTROLLER_DELEGATE_H_
