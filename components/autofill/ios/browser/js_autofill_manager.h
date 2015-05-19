// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_JS_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_JS_AUTOFILL_MANAGER_H_

#include "base/ios/block_types.h"
#include "components/autofill/core/common/autofill_constants.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

@class CRWJSInjectionReceiver;

// Loads the JavaScript file, autofill_controller.js, which contains form
// parsing and autofill functions.
@interface JsAutofillManager : CRWJSInjectionManager

// Extracts forms from a web page. Only forms with at least |requiredFields|
// fields and the appropriate attribute requirements are extracted.
// |completionHandler| is called with the JSON string of forms of a web page.
// |completionHandler| cannot be nil.
- (void)fetchFormsWithRequirements:(autofill::RequirementsMask)requirements
        minimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                 completionHandler:(void (^)(NSString*))completionHandler;

// Stores the current active element. This is used to make the element active
// again in case the web view loses focus when a dialog is presented over it.
- (void)storeActiveElement;

// Clears the current active element.
- (void)clearActiveElement;

// Fills the data in JSON string |dataString| into the active form field, then
// executes the |completionHandler|. The active form field is either
// document.activeElement or the field stored by a call to storeActiveElement.
// non-null.
- (void)fillActiveFormField:(NSString*)dataString
          completionHandler:(ProceduralBlock)completionHandler;

// Fills a number of fields in the same named form.
// |completionHandler| is called after the forms are filled. |completionHandler|
// cannot be nil.
- (void)fillForm:(NSString*)dataString
    completionHandler:(ProceduralBlock)completionHandler;

// Dispatches the autocomplete event to the form element with the given
// |formName|.
- (void)dispatchAutocompleteEvent:(NSString*)formName;

// Dispatches the autocomplete error event to the form element with the given
// |formName|, supplying the given reason.
- (void)dispatchAutocompleteErrorEvent:(NSString*)formName
                            withReason:(NSString*)reason;

// Marks up the form with autofill field prediction data (diagnostic tool).
- (void)fillPredictionData:(NSString*)dataString;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_JS_AUTOFILL_MANAGER_H_
