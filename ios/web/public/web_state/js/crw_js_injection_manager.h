// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_INJECTION_MANAGER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_INJECTION_MANAGER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state/js/crw_js_injection_evaluator.h"

@class CRWJSInjectionReceiver;

// This class defines the abstract interface for managing JavaScript
// Injection into a UIWebView.
@interface CRWJSInjectionManager : NSObject

// Designated initializer. Initializes the object with the |receiver|.
- (id)initWithReceiver:(CRWJSInjectionReceiver*)receiver;

// The array of |CRWJSInjectionManager| this class depends on. Default to be
// empty array. Note circular dependency is not allowed, which will cause stack
// overflow in dependency related computation such as -allDependencies.
- (NSArray*)directDependencies;

// Returns a list of all the CRWJSInjectionManagers required by this manager,
// that is, this manager and the managers it directly or indirectly depends on.
// The list is ordered in such a way that any CRWJSInjectionManager in the list
// only depends on those appear before it in the list.
- (NSArray*)allDependencies;

// Returns whether JavaScript has already been injected into the receiver.
- (BOOL)hasBeenInjected;

// Injects JavaScript at |self.scriptPath| into the receiver object if it is
// missing. It also injects the dependencies' JavaScript if they are missing.
- (void)inject;

// Evaluates the provided JavaScript expression, slightly deferred. Designed for
// scripts where the chance of crwebinvoke:// being triggered indirectly is
// high, and that aren't required to return a value.
- (void)deferredEvaluate:(NSString*)script;

// Evaluate the provided JavaScript asynchronously calling completionHandler
// after execution. The |completionHandler| can be nil.
- (void)evaluate:(NSString*)script
    stringResultHandler:(web::JavaScriptCompletion)completionHandler;

@end

@interface CRWJSInjectionManager (ProtectedMethods)

// The injection receiver used to evaluate JavaScript.
- (CRWJSInjectionReceiver*)receiver;

// Path for the resource in the application bundle of type "js" that needs to
// injected for this manager.
// Subclasses must override this method to return the path to the JavaScript
// that needs to be injected.
- (NSString*)scriptPath;

// The JavaScript function that returns the JavaScript constant of undefined
// if the JavaScript has not been injected. Default to be nil. Subclasses
// should override this if their script should only be injected into a page
// once.
- (NSString*)presenceBeacon;

// Returns the content that should be injected. This is called every time
// injection content is needed; by default is uses a cached copy of
// staticInjectionContent.
// Subclasses should override this only if the content needs to be dynamic
// rather than cached, otherwise they should override staticInjectionContent.
- (NSString*)injectionContent;

// Returns an autoreleased string that is the JavaScript to be injected into
// the receiver object. By default this returns the contents of the script file;
// subclasses can override this if they need to get a static script from some
// other source.
// The return value from this method will be cached; if dynamic script content
// is necessary, override injectionContent instead.
- (NSString*)staticInjectionContent;

// Injects dependencies if they are missing.
- (void)injectDependenciesIfMissing;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_INJECTION_MANAGER_H_
