// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_CONTROLLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVAutofillSuggestion;
@protocol CWVAutofillControllerDelegate;

CWV_EXPORT
// Exposes features that allow autofilling html forms. May include autofilling
// of single fields, address forms, or credit card forms.
@interface CWVAutofillController : NSObject

// Delegate to receive autofill callbacks.
@property(nonatomic, weak, nullable) id<CWVAutofillControllerDelegate> delegate;

- (instancetype)init NS_UNAVAILABLE;

// Clears the html form element with the 'name' attribute equal to |formName|.
// No-op if no such form is found.
// |completionHandler| will only be called on success.
- (void)clearFormWithName:(NSString*)formName
        completionHandler:(nullable void (^)(void))completionHandler;

// For the field named |fieldName| in the form named |formName|, fetches
// suggestions that can be used to autofill.
// No-op if no such form and field can be found in the current page.
// |completionHandler| will only be called on success.
- (void)fetchSuggestionsForFormWithName:(NSString*)formName
                              fieldName:(NSString*)fieldName
                      completionHandler:
                          (void (^)(NSArray<CWVAutofillSuggestion*>*))
                              completionHandler;

// Takes the |suggestion| and finds the form matching its |formName| and
// |fieldName| property. If found, autofills the values in to the page.
// No-op if no such form and field can be found in the current page.
// |completionHandler| will only be called on success.
- (void)fillSuggestion:(CWVAutofillSuggestion*)suggestion
     completionHandler:(nullable void (^)(void))completionHandler;

// Deletes a suggestion from the data store. This suggestion will not be fetched
// again.
- (void)removeSuggestion:(CWVAutofillSuggestion*)suggestion;

// Changes focus to the previous sibling of the currently focused field.
// No-op if no field is currently focused or if previous field is not available.
- (void)focusPreviousField;

// Changes focus to the next sibling of the currently focused field.
// No-op if no field is currently focused or if next field is not available.
- (void)focusNextField;

// Checks if there are next or previous fields for focusing.
// |previous| and |next| indiciates if it is possible to focus.
- (void)checkIfPreviousAndNextFieldsAreAvailableForFocusWithCompletionHandler:
    (void (^)(BOOL previous, BOOL next))completionHandler;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_CONTROLLER_H_
