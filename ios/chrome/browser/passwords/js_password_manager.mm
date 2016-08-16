// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/js_password_manager.h"

#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"

namespace {
// Sanitizes |JSONString| and wraps it in quotes so it can be injected safely in
// JavaScript.
NSString* JSONEscape(NSString* JSONString) {
  return base::SysUTF8ToNSString(
      base::GetQuotedJSONString(base::SysNSStringToUTF8(JSONString)));
}
}  // namespace

@interface JsPasswordManager ()
// Injects a script that does two things:
// 1. Injects password controller JavaScript in the page.
// 2. Extracts the _submitted_ password form data from the DOM on the page.
// The result is returned in |completionHandler|.
// |completionHandler| cannot be nil.
- (void)evaluateExtraScript:(NSString*)script
          completionHandler:(void (^)(NSString*))completionHandler;
@end

@implementation JsPasswordManager

- (void)findPasswordFormsWithCompletionHandler:
    (void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  [self evaluateExtraScript:@"__gCrWeb.findPasswordForms()"
          completionHandler:completionHandler];
}

- (void)extractForm:(NSString*)formName
      completionHandler:(void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  NSString* extra = [NSString
      stringWithFormat:@"__gCrWeb.getPasswordForm(%@)", JSONEscape(formName)];
  [self evaluateExtraScript:extra completionHandler:completionHandler];
}

- (void)fillPasswordForm:(NSString*)JSONString
            withUsername:(NSString*)username
                password:(NSString*)password
       completionHandler:(void (^)(BOOL))completionHandler {
  DCHECK(completionHandler);
  NSString* script = [NSString
      stringWithFormat:@"__gCrWeb.fillPasswordForm(%@, %@, %@)", JSONString,
                       JSONEscape(username), JSONEscape(password)];
  [self executeJavaScript:script completionHandler:^(id result, NSError*) {
    completionHandler([result isEqual:@YES]);
  }];
}

- (void)clearAutofilledPasswordsInForm:(NSString*)formName
                     completionHandler:(void (^)(BOOL))completionHandler {
  NSString* script =
      [NSString stringWithFormat:@"__gCrWeb.clearAutofilledPasswords(%@)",
                                 JSONEscape(formName)];
  [self executeJavaScript:script completionHandler:^(id result, NSError*) {
    completionHandler([result isEqual:@YES]);
  }];
}

- (void)fillPasswordForm:(NSString*)formName
   withGeneratedPassword:(NSString*)password
       completionHandler:(void (^)(BOOL))completionHandler {
  NSString* script =
      [NSString stringWithFormat:
                    @"__gCrWeb.fillPasswordFormWithGeneratedPassword(%@, %@)",
                    JSONEscape(formName), JSONEscape(password)];
  [self executeJavaScript:script completionHandler:^(id result, NSError*) {
    if (completionHandler)
      completionHandler([result isEqual:@YES]);
  }];
}

#pragma mark -
#pragma mark ProtectedMethods

- (NSString*)scriptPath {
  return @"password_controller";
}

#pragma mark -
#pragma mark Private

- (void)evaluateExtraScript:(NSString*)script
          completionHandler:(void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  NSString* JS = [[self injectionContent] stringByAppendingString:script];
  [self executeJavaScript:JS completionHandler:^(id result, NSError*) {
    completionHandler(base::mac::ObjCCastStrict<NSString>(result));
  }];
}

@end
