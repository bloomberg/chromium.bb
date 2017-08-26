// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_request.h"

#import "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/web/public/web_state/web_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The hostname to use for JavaScript dialogs when the origin URL is invalid.
const char kAboutNullHostname[] = "about:null";
}  // namespace

@interface JavaScriptDialogRequest () {
  // The callback passed upon initialization.
  web::DialogClosedCallback _callback;
  // Whether |_callback| has been run;
  BOOL _callbackHasBeenExecuted;
}

// Private initializer used by factory method.
- (instancetype)initWithWebState:(web::WebState*)webState
                            type:(web::JavaScriptDialogType)type
                       originURL:(const GURL&)originURL
                         message:(NSString*)message
               defaultPromptText:(NSString*)defaultPromptText
                        callback:(const web::DialogClosedCallback&)callback;

// Returns the title to use when displaying dialogs from |originURL|.
+ (NSString*)titleForDialogFromOrigin:(const GURL&)originURL;

@end

@implementation JavaScriptDialogRequest

@synthesize webState = _webState;
@synthesize type = _type;
@synthesize title = _title;
@synthesize message = _message;
@synthesize defaultPromptText = _defaultPromptText;

- (instancetype)initWithWebState:(web::WebState*)webState
                            type:(web::JavaScriptDialogType)type
                       originURL:(const GURL&)originURL
                         message:(NSString*)message
               defaultPromptText:(NSString*)defaultPromptText
                        callback:(const web::DialogClosedCallback&)callback {
  if ((self = [super init])) {
    _webState = webState;
    _type = type;
    _title = [[self class] titleForDialogFromOrigin:originURL];
    _message = message;
    _defaultPromptText = defaultPromptText;
    _callback = callback;
  }
  return self;
}

- (void)dealloc {
  DCHECK(_callbackHasBeenExecuted);
}

#pragma mark - Public

+ (instancetype)requestWithWebState:(web::WebState*)webState
                               type:(web::JavaScriptDialogType)type
                          originURL:(const GURL&)originURL
                            message:(NSString*)message
                  defaultPromptText:(NSString*)defaultPromptText
                           callback:(const web::DialogClosedCallback&)callback {
  return [[[self class] alloc] initWithWebState:webState
                                           type:type
                                      originURL:originURL
                                        message:message
                              defaultPromptText:defaultPromptText
                                       callback:callback];
}

- (void)finishRequestWithSuccess:(BOOL)success userInput:(NSString*)userInput {
  if (_callbackHasBeenExecuted)
    return;
  _callback.Run(success, userInput);
  _callbackHasBeenExecuted = YES;
}

#pragma mark -

+ (NSString*)titleForDialogFromOrigin:(const GURL&)originURL {
  NSString* hostname = originURL.is_valid()
                           ? base::SysUTF8ToNSString(originURL.host())
                           : base::SysUTF8ToNSString(kAboutNullHostname);
  return l10n_util::GetNSStringF(IDS_JAVASCRIPT_MESSAGEBOX_TITLE,
                                 base::SysNSStringToUTF16(hostname));
}

@end
