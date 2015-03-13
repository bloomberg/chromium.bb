// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_CONTROLLER_OBSERVER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_CONTROLLER_OBSERVER_H_

#import <Foundation/Foundation.h>
#include <string>

@class CRWWebController;
@class CRWWebViewProxy;
class GURL;
@class UIWebView;

namespace base {
class DictionaryValue;
}

// NOTE: When adding new methods to CRWWebControllerObserver, consider adding
// them to WebStateObserver instead if they need to be surfaced to the public
// API.
@protocol CRWWebControllerObserver<NSObject>

@optional

// Supplies a text prefix to the CRWWebController to indicate which commands the
// observer should receive using the handleCommand message.
// Called only as the observer is added to its parent CRWWebController.
@property(nonatomic, readonly) NSString* commandPrefix;

// Called when the current page is loaded.
// DEPRECATED: Use WebStateObserver instead.
- (void)pageLoaded:(CRWWebController*)webController;

// Called when a form is being submitted.
- (void)documentSubmit:(CRWWebController*)webController
              formName:(const std::string&)formName
       userInteraction:(BOOL)userInteraction;

// Called when the user is typing on a form field, with |error| indicating if
// there is any error when parsing the form field information. Currently these
// events will not be sent if the Disable Autofill experiment is set.
- (void)formActivity:(CRWWebController*)webController
            formName:(const std::string&)formName
           fieldName:(const std::string&)fieldName
                type:(const std::string&)type
               value:(const std::string&)value
               error:(bool)error;

// Identical to |formActivity:formName:fieldName:type:value:error:|, but
// indicates that the activity was triggered by typing the key specified by
// |keyCode|.
- (void)formActivity:(CRWWebController*)webController
            formName:(const std::string&)formName
           fieldName:(const std::string&)fieldName
                type:(const std::string&)type
               value:(const std::string&)value
             keyCode:(int)keyCode
               error:(bool)error;

// The page requested autocomplete.
- (void)requestAutocomplete:(CRWWebController*)webController
                  sourceURL:(const GURL&)sourceURL
                   formName:(const std::string&)formName
            userInteraction:(BOOL)userInteraction;

// Called when the web controller is about to close.
- (void)webControllerWillClose:(CRWWebController*)webController;

// Handle the command from page scripts. Return NO if the command was known to
// be invalid. This will cause the page to be reset as a security precaution.
// DEPRECATED: Use WebState::ScriptCommandCallback instead.
- (BOOL)handleCommand:(const base::DictionaryValue&)command
        webController:(CRWWebController*)webController
    userIsInteracting:(BOOL)userIsInteracting
            originURL:(const GURL&)originURL;

// Gives CRWWebControllerObservers access to the CRWWebViewProxy.
- (void)setWebViewProxy:(CRWWebViewProxy*)webView
             controller:(CRWWebController*)webController;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_CONTROLLER_OBSERVER_H_
