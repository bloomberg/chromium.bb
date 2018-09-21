// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_autofill_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#include "ios/web/public/web_state/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The timeout for any JavaScript call in this file.
// It is only used if IsAutofillIFrameMessagingEnabled is enabled.
const int64_t kJavaScriptExecutionTimeoutInSeconds = 5;
}

@interface JsAutofillManager ()

// Executes the JavaScript function using WebState of WebFrame functions
// depending on the value of IsAutofillIFrameMessagingEnabled.
// If |completionHandler| is not nil, it will be called when the result of the
// command is received, or immediatly if the command cannot be executed.
// It will call the either |executeJavaScriptFunctionInWebFrameWithName:...|
// or |executeJavaScriptFunctionInWebStateWithName:...| depending on the
// IsAutofillIFrameMessagingEnabled value.
- (void)
executeJavaScriptFunctionWithName:(const std::string&)name
                       parameters:(const std::vector<base::Value>&)parameters
                          inFrame:(web::WebFrame*)frame
                completionHandler:(void (^)(NSString*))completionHandler;

- (void)executeInWebFrameJavaScriptFunctionWithName:(const std::string&)name
                                         parameters:
                                             (const std::vector<base::Value>&)
                                                 parameters
                                            inFrame:(web::WebFrame*)frame
                                  completionHandler:
                                      (void (^)(NSString*))completionHandler;
- (void)executeInWebStateJavaScriptFunctionWithName:(const std::string&)name
                                         parameters:
                                             (const std::vector<base::Value>&)
                                                 parameters
                                  completionHandler:
                                      (void (^)(NSString*))completionHandler;

@end

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

- (void)
executeJavaScriptFunctionWithName:(const std::string&)name
                       parameters:(const std::vector<base::Value>&)parameters
                          inFrame:(web::WebFrame*)frame
                completionHandler:(void (^)(NSString*))completionHandler {
  if (autofill::switches::IsAutofillIFrameMessagingEnabled()) {
    [self executeInWebFrameJavaScriptFunctionWithName:name
                                           parameters:parameters
                                              inFrame:frame
                                    completionHandler:completionHandler];
  } else {
    [self executeInWebStateJavaScriptFunctionWithName:name
                                           parameters:parameters
                                    completionHandler:completionHandler];
  }
}

- (void)executeInWebFrameJavaScriptFunctionWithName:(const std::string&)name
                                         parameters:
                                             (const std::vector<base::Value>&)
                                                 parameters
                                            inFrame:(web::WebFrame*)frame
                                  completionHandler:
                                      (void (^)(NSString*))completionHandler {
  if (!frame) {
    if (completionHandler) {
      completionHandler(nil);
    }
    return;
  }
  DCHECK(frame->CanCallJavaScriptFunction());
  if (completionHandler) {
    bool called = frame->CallJavaScriptFunction(
        name, parameters, base::BindOnce(^(const base::Value* res) {
          if (!res || !res->is_string()) {
            completionHandler(nil);
            return;
          }
          completionHandler(base::SysUTF8ToNSString(res->GetString()));
        }),
        base::TimeDelta::FromSeconds(kJavaScriptExecutionTimeoutInSeconds));
    if (!called) {
      completionHandler(nil);
    }
  } else {
    frame->CallJavaScriptFunction(name, parameters);
  }
}

- (void)executeInWebStateJavaScriptFunctionWithName:(const std::string&)name
                                         parameters:
                                             (const std::vector<base::Value>&)
                                                 parameters
                                  completionHandler:
                                      (void (^)(NSString*))completionHandler {
  std::string function_name = "__gCrWeb." + name;
  NSMutableArray* JSONParameters = [[NSMutableArray alloc] init];
  for (auto& value : parameters) {
    std::string dataString;
    base::JSONWriter::Write(value, &dataString);
    [JSONParameters addObject:base::SysUTF8ToNSString(dataString)];
  }
  NSString* command = [NSString
      stringWithFormat:@"%s(%@);", function_name.c_str(),
                       [JSONParameters componentsJoinedByString:@", "]];
  if (completionHandler) {
    [_receiver executeJavaScript:command
               completionHandler:^(id result, NSError* error) {
                 completionHandler(base::mac::ObjCCastStrict<NSString>(result));
               }];
  } else {
    [_receiver executeJavaScript:command completionHandler:nil];
  }
}

- (void)addJSDelayInFrame:(web::WebFrame*)frame {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          autofill::switches::kAutofillIOSDelayBetweenFields)) {
    std::string delayString = command_line->GetSwitchValueASCII(
        autofill::switches::kAutofillIOSDelayBetweenFields);
    int commandLineDelay = 0;
    if (base::StringToInt(delayString, &commandLineDelay)) {
      std::vector<base::Value> parameters;
      parameters.push_back(base::Value(commandLineDelay));
      [self executeJavaScriptFunctionWithName:"autofill.setDelay"
                                   parameters:parameters
                                      inFrame:frame
                            completionHandler:nil];
    }
  }
}

- (void)fetchFormsWithMinimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                                         inFrame:(web::WebFrame*)frame
                               completionHandler:
                                   (void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);

  bool restrictUnownedFieldsToFormlessCheckout = base::FeatureList::IsEnabled(
      autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout);
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(static_cast<int>(requiredFieldsCount)));
  parameters.push_back(base::Value(restrictUnownedFieldsToFormlessCheckout));
  [self executeJavaScriptFunctionWithName:"autofill.extractForms"
                               parameters:parameters
                                  inFrame:frame
                        completionHandler:completionHandler];
}

#pragma mark -
#pragma mark ProtectedMethods

- (void)fillActiveFormField:(std::unique_ptr<base::Value>)data
                    inFrame:(web::WebFrame*)frame
          completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(data);
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  [self executeJavaScriptFunctionWithName:"autofill.fillActiveFormField"
                               parameters:parameters
                                  inFrame:frame
                        completionHandler:^(NSString*) {
                          if (completionHandler)
                            completionHandler();
                        }];
}

- (void)toggleTrackingFormMutations:(BOOL)state inFrame:(web::WebFrame*)frame {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(state ? 200 : 0));
  [self executeJavaScriptFunctionWithName:"form.trackFormMutations"
                               parameters:parameters
                                  inFrame:frame
                        completionHandler:nil];
}

- (void)toggleTrackingUserEditedFields:(BOOL)state
                               inFrame:(web::WebFrame*)frame {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(static_cast<bool>(state)));
  [self executeJavaScriptFunctionWithName:"form.toggleTrackingUserEditedFields"
                               parameters:parameters
                                  inFrame:frame
                        completionHandler:nil];
}

- (void)fillForm:(std::unique_ptr<base::Value>)data
    forceFillFieldIdentifier:(NSString*)forceFillFieldIdentifier
                     inFrame:(web::WebFrame*)frame
           completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(data);
  DCHECK(completionHandler);
  std::string fieldIdentifier =
      forceFillFieldIdentifier
          ? base::SysNSStringToUTF8(forceFillFieldIdentifier)
          : "null";
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  parameters.push_back(base::Value(fieldIdentifier));
  [self executeJavaScriptFunctionWithName:"autofill.fillForm"
                               parameters:parameters
                                  inFrame:frame
                        completionHandler:^(NSString*) {
                          completionHandler();
                        }];
}

- (void)clearAutofilledFieldsForFormName:(NSString*)formName
                         fieldIdentifier:(NSString*)fieldIdentifier
                                 inFrame:(web::WebFrame*)frame
                       completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(completionHandler);
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(base::SysNSStringToUTF8(formName)));
  parameters.push_back(base::Value(base::SysNSStringToUTF8(fieldIdentifier)));
  [self executeJavaScriptFunctionWithName:"autofill.clearAutofilledFields"
                               parameters:parameters
                                  inFrame:frame
                        completionHandler:^(NSString*) {
                          completionHandler();
                        }];
}

- (void)fillPredictionData:(std::unique_ptr<base::Value>)data
                   inFrame:(web::WebFrame*)frame {
  DCHECK(data);
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  [self executeJavaScriptFunctionWithName:"autofill.fillPredictionData"
                               parameters:parameters
                                  inFrame:frame
                        completionHandler:nil];
}

@end
