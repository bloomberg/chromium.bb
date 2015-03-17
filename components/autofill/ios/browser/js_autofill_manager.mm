// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_autofill_manager.h"

#include "base/format_macros.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#import "ios/web/public/web_state/js/crw_js_early_script_manager.h"

@implementation JsAutofillManager

- (void)fetchFormsWithRequirements:(autofill::RequirementsMask)requirements
        minimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                 completionHandler:(void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  // Convert from C++ enum to JS enum.
  NSString* requirementsJS = nil;
  switch (requirements) {
    case autofill::REQUIRE_NONE:
      requirementsJS = @"__gCrWeb.autofill.REQUIREMENTS_MASK_NONE";
      break;
    case autofill::REQUIRE_AUTOCOMPLETE:
      requirementsJS =
          @"__gCrWeb.autofill.REQUIREMENTS_MASK_REQUIRE_AUTOCOMPLETE";
      break;
  }
  DCHECK(requirementsJS);

  NSString* extractFormsJS = [NSString
      stringWithFormat:@"__gCrWeb.autofill.extractForms(%" PRIuNS ", %@);",
                       requiredFieldsCount, requirementsJS];
  [self evaluate:extractFormsJS
      stringResultHandler:^(NSString* result, NSError*) {
        completionHandler(result);
      }];
}

#pragma mark -
#pragma mark ProtectedMethods

- (NSString*)scriptPath {
  return @"autofill_controller";
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb.autofill";
}

- (NSArray*)directDependencies {
  return @[ [CRWJSEarlyScriptManager class] ];
}

- (void)fillActiveFormField:(NSString*)dataString
          completionHandler:(ProceduralBlock)completionHandler {
  web::JavaScriptCompletion resultHandler = ^void(NSString*, NSError*) {
    completionHandler();
  };

  NSString* js =
      [NSString stringWithFormat:@"__gCrWeb.autofill.fillActiveFormField(%@);",
                                 dataString];
  [self evaluate:js stringResultHandler:resultHandler];
}

- (void)fillForm:(NSString*)dataString
    completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(completionHandler);
  NSString* fillFormJS = [NSString
      stringWithFormat:@"__gCrWeb.autofill.fillForm(%@);", dataString];
  id stringResultHandler = ^(NSString*, NSError*) {
    completionHandler();
  };
  return [self evaluate:fillFormJS stringResultHandler:stringResultHandler];
}

- (void)dispatchAutocompleteEvent:(NSString*)formName {
  NSString* dispatchAutocompleteEventJS = [NSString
      stringWithFormat:@"__gCrWeb.autofill.dispatchAutocompleteEvent(%s);",
                       base::GetQuotedJSONString([formName UTF8String])
                           .c_str()];
  [self evaluate:dispatchAutocompleteEventJS stringResultHandler:nil];
}

- (void)dispatchAutocompleteErrorEvent:(NSString*)formName
                            withReason:(NSString*)reason {
  NSString* autocompleteErrorJS = [NSString
      stringWithFormat:
          @"__gCrWeb.autofill.dispatchAutocompleteErrorEvent(%s, %s);",
          base::GetQuotedJSONString([formName UTF8String]).c_str(),
          base::GetQuotedJSONString([reason UTF8String]).c_str()];
  [self evaluate:autocompleteErrorJS stringResultHandler:nil];
}

- (void)fillPredictionData:(NSString*)dataString {
  [self deferredEvaluate:
            [NSString
                stringWithFormat:@"__gCrWeb.autofill.fillPredictionData(%@);",
                                 dataString]];
}

@end
