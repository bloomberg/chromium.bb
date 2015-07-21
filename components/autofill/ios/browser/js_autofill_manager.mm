// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_autofill_manager.h"

#include "base/format_macros.h"
#include "base/json/string_escape.h"
#include "base/logging.h"

@implementation JsAutofillManager

- (void)fetchFormsWithMinimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                               completionHandler:
                                   (void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  NSString* extractFormsJS = [NSString
      stringWithFormat:@"__gCrWeb.autofill.extractForms(%" PRIuNS ");",
                       requiredFieldsCount];
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

- (void)storeActiveElement {
  NSString* js = @"__gCrWeb.autofill.storeActiveElement()";
  [self evaluate:js stringResultHandler:nil];
}

- (void)clearActiveElement {
  NSString* js = @"__gCrWeb.autofill.clearActiveElement()";
  [self evaluate:js stringResultHandler:nil];
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
         onlyFillEmpty:(BOOL)onlyFillEmpty
    forceFillFieldName:(NSString*)forceFillFieldName
         styleElements:(BOOL)styleElements
     completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(completionHandler);
  std::string fieldName =
      forceFillFieldName
          ? base::GetQuotedJSONString([forceFillFieldName UTF8String])
          : "null";
  NSString* fillFormJS = [NSString
      stringWithFormat:@"__gCrWeb.autofill.fillForm(%@, %s, %s, %s);",
                       dataString, onlyFillEmpty ? "true" : "false",
                       fieldName.c_str(), styleElements ? "true" : "false"];
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
