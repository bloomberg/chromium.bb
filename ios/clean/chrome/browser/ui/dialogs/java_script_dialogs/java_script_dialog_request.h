// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_JAVA_SCRIPT_DIALOG_STATE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_JAVA_SCRIPT_DIALOG_STATE_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/java_script_dialog_callback.h"
#include "ios/web/public/java_script_dialog_type.h"

class GURL;

namespace web {
class WebState;
}

// A container object encapsulating all the state necessary to support a
// JavaScriptDialogCoordinator.  This object also owns the WebKit completion
// block that will throw an exception if it is deallocated before being
// executed. |-runCallbackWithSuccess:userInput:| must be executed once in the
// lifetime of every JavaScriptDialogRequest.
@interface JavaScriptDialogRequest : NSObject

// Factory method to create JavaScriptDialogRequests from the given input.
+ (instancetype)requestWithWebState:(web::WebState*)webState
                               type:(web::JavaScriptDialogType)type
                          originURL:(const GURL&)originURL
                            message:(NSString*)message
                  defaultPromptText:(NSString*)defaultPromptText
                           callback:(const web::DialogClosedCallback&)callback;

// The WebState displaying this dialog.
@property(nonatomic, readonly) web::WebState* webState;

// The type of dialog to display.
@property(nonatomic, readonly) web::JavaScriptDialogType type;

// The title to use for the dialog.
@property(nonatomic, readonly, strong) NSString* title;

// The dialog message supplied by the page.
@property(nonatomic, readonly, strong) NSString* message;

// The default text to display in the text field for prompt dialogs.
@property(nonatomic, readonly, strong) NSString* defaultPromptText;

// Finishes the request with the proved parameters.  |success| indicates whether
// the user tapped on the OK or Cancel button of the dialog.  For prompts,
// |userInput| should be the input into the text field.  For cancelled prompts
// or for alerts/confirmations, |userInput| should be nil.
- (void)finishRequestWithSuccess:(BOOL)success userInput:(NSString*)userInput;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_JAVA_SCRIPT_DIALOG_STATE_H_
