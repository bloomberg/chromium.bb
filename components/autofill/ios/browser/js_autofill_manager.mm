// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_autofill_manager.h"

#include "base/format_macros.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation JsAutofillManager {
  // The injection receiver used to evaluate JavaScript.
  CRWJSInjectionReceiver* _receiver;
}

- (instancetype)initWithReceiver:(CRWJSInjectionReceiver*)receiver {
  DCHECK(receiver);
  self = [super init];
  if (self) {
    _receiver = receiver;
  }
  return self;
}

- (void)fetchFormsWithMinimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                               completionHandler:
                                   (void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  NSString* extractFormsJS = [NSString
      stringWithFormat:@"__gCrWeb.autofill.extractForms(%" PRIuNS ");",
                       requiredFieldsCount];
  [_receiver executeJavaScript:extractFormsJS
             completionHandler:^(id result, NSError*) {
               completionHandler(base::mac::ObjCCastStrict<NSString>(result));
             }];
}

#pragma mark -
#pragma mark ProtectedMethods

- (void)fillActiveFormField:(NSString*)dataString
          completionHandler:(ProceduralBlock)completionHandler {
  NSString* script =
      [NSString stringWithFormat:@"__gCrWeb.autofill.fillActiveFormField(%@);",
                                 dataString];
  [_receiver executeJavaScript:script
             completionHandler:^(id, NSError*) {
               completionHandler();
             }];
}

- (void)fillForm:(NSString*)dataString
    forceFillFieldName:(NSString*)forceFillFieldName
     completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(completionHandler);
  std::string fieldName =
      forceFillFieldName
          ? base::GetQuotedJSONString([forceFillFieldName UTF8String])
          : "null";
  NSString* fillFormJS =
      [NSString stringWithFormat:@"__gCrWeb.autofill.fillForm(%@, %s);",
                                 dataString, fieldName.c_str()];
  [_receiver executeJavaScript:fillFormJS
             completionHandler:^(id, NSError*) {
               completionHandler();
             }];
}

- (void)clearAutofilledFieldsForFormNamed:(NSString*)formName
                        completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(completionHandler);
  NSString* script =
      [NSString stringWithFormat:
                    @"__gCrWeb.autofill.clearAutofilledFields(%s);",
                    base::GetQuotedJSONString([formName UTF8String]).c_str()];
  [_receiver executeJavaScript:script
             completionHandler:^(id, NSError*) {
               completionHandler();
             }];
}

- (void)fillPredictionData:(NSString*)dataString {
  NSString* script =
      [NSString stringWithFormat:@"__gCrWeb.autofill.fillPredictionData(%@);",
                                 dataString];
  [_receiver executeJavaScript:script completionHandler:nil];
}

@end
