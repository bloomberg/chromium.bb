// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_invoke_parameter_queue.h"

#import "base/mac/scoped_nsobject.h"
#include "url/gurl.h"

@implementation CRWJSInvokeParameters {
  base::scoped_nsobject<NSString> _commandString;
  BOOL _userIsInteracting;
  base::scoped_nsobject<NSString> _windowId;
  GURL _originURL;
}

@synthesize userIsInteracting = _userIsInteracting;

- (id)initWithCommandString:(NSString*)commandString
          userIsInteracting:(BOOL)userIsInteracting
                  originURL:(const GURL&)originURL
                forWindowId:(NSString*)windowId {
  if ((self = [super init])) {
    _commandString.reset([commandString copy]);
    _userIsInteracting = userIsInteracting;
    _windowId.reset([windowId copy]);
    _originURL = originURL;
  }
  return self;
}

- (NSString*)commandString {
  return _commandString.get();
}

- (NSString*)windowId {
  return _windowId.get();
}

- (const GURL&)originURL {
  return _originURL;
}

@end

@implementation CRWJSInvokeParameterQueue {
  base::scoped_nsobject<NSMutableArray> _queue;
}

- (id)init {
  if ((self = [super init])) {
    // Under normal circumstainces there will be maximum one message queued.
    _queue.reset([[NSMutableArray arrayWithCapacity:1] retain]);
  }
  return self;
}

- (BOOL)isEmpty {
  return [_queue count] == 0;
}

- (NSUInteger)queueLength {
  return [_queue count];
}

- (void)addCommandString:(NSString*)commandString
       userIsInteracting:(BOOL)userIsInteracting
               originURL:(const GURL&)originURL
             forWindowId:(NSString*)windowId {
  base::scoped_nsobject<CRWJSInvokeParameters> invokeParameters(
      [[CRWJSInvokeParameters alloc] initWithCommandString:commandString
                                         userIsInteracting:userIsInteracting
                                                 originURL:originURL
                                               forWindowId:windowId]);
  [_queue addObject:invokeParameters];
}

- (void)removeCommandString:(NSString*)commandString {
  NSMutableArray* commandsToRemove = [NSMutableArray array];
  for (CRWJSInvokeParameters* params in _queue.get()) {
    NSRange range =
        [[params commandString] rangeOfString:commandString
                                      options:NSCaseInsensitiveSearch];
    if (range.location != NSNotFound)
      [commandsToRemove addObject:params];
  }
  [_queue removeObjectsInArray:commandsToRemove];
}

- (CRWJSInvokeParameters*)popInvokeParameters {
  if (![_queue count])
    return nil;
  CRWJSInvokeParameters* invokeParameters =
      [[[_queue objectAtIndex:0] retain] autorelease];
  [_queue removeObjectAtIndex:0];
  return invokeParameters;
}

@end
