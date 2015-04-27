// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/web_state/js/page_script_util.h"

@implementation CRWJSInjectionManager {
  // JS to inject into the page. This may be nil if it has been purged due to
  // low memory.
  base::scoped_nsobject<NSString> _injectObject;
  // An object the can receive JavaScript injection.
  CRWJSInjectionReceiver* _receiver;  // Weak.
}

- (id)initWithReceiver:(CRWJSInjectionReceiver*)receiver {
  DCHECK(receiver);
  self = [super init];
  if (self) {
    _receiver = receiver;
    // Register for low-memory warnings.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(lowMemoryWarning:)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (BOOL)hasBeenInjected {
  DCHECK(self.presenceBeacon);
  return [_receiver scriptHasBeenInjectedForClass:[self class]
                                   presenceBeacon:self.presenceBeacon];
}

- (void)inject {
  if ([self hasBeenInjected])
    return;
  [self injectDependenciesIfMissing];
  [_receiver injectScript:[self injectionContent] forClass:[self class]];
  DCHECK([self hasBeenInjected]);
}

- (void)lowMemoryWarning:(NSNotification*)notify {
  _injectObject.reset();
}

- (void)deferredEvaluate:(NSString*)script {
  NSString* deferredScript = [NSString
      stringWithFormat:@"window.setTimeout(function() {%@}, 0)", script];
  [self evaluate:deferredScript stringResultHandler:nil];
}

- (void)evaluate:(NSString*)script
    stringResultHandler:(web::JavaScriptCompletion)completionHandler {
  [_receiver evaluateJavaScript:script stringResultHandler:completionHandler];
}

- (NSArray*)directDependencies {
  return @[];
}

- (NSArray*)allDependencies {
  NSMutableArray* allDendencies = [NSMutableArray array];
  for (Class dependencyClass in [self directDependencies]) {
    CRWJSInjectionManager* dependency =
        [_receiver instanceOfClass:dependencyClass];
    NSArray* list = [dependency allDependencies];
    for (CRWJSInjectionManager* manager in list) {
      if (![allDendencies containsObject:manager])
        [allDendencies addObject:manager];
    }
  }
  [allDendencies addObject:self];
  return allDendencies;
}

#pragma mark -
#pragma mark ProtectedMethods

- (CRWJSInjectionReceiver*)receiver {
  return _receiver;
}

- (NSString*)scriptPath {
  NOTREACHED();
  return nil;
}

- (NSString*)presenceBeacon {
  return nil;
}

- (NSString*)injectionContent {
  if (!_injectObject)
    _injectObject.reset([[self staticInjectionContent] copy]);
  return _injectObject.get();
}

- (NSString*)staticInjectionContent {
  return web::GetPageScript([self scriptPath]);
}

- (void)injectDependenciesIfMissing {
  for (Class dependencyClass in [self directDependencies]) {
    CRWJSInjectionManager* dependency =
        [_receiver instanceOfClass:dependencyClass];
    [dependency inject];
  }
}

@end
