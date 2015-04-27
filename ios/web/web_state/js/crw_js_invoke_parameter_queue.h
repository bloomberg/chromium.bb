// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_JS_CRW_JS_INVOKE_PARAMETER_QUEUE_H_
#define IOS_WEB_WEB_STATE_JS_CRW_JS_INVOKE_PARAMETER_QUEUE_H_

#import <UIKit/UIKit.h>

class GURL;

// Manages access to individual invoke parameters.
@interface CRWJSInvokeParameters : NSObject

// The designated initializer.
- (id)initWithCommandString:(NSString*)commandString
          userIsInteracting:(BOOL)userIsInteracting
                  originURL:(const GURL&)originURL
                forWindowId:(NSString*)windowId;

// An escaped string with commands requested by JavaScript.
@property(nonatomic, readonly) NSString* commandString;

// Whether the user was interacting when the command was issued.
@property(nonatomic, readonly) BOOL userIsInteracting;

// Returns window id of the originating window.
@property(nonatomic, readonly) NSString* windowId;

// Returns URL that was current when the crwebinvoke was issued.
@property(nonatomic, readonly) const GURL& originURL;

@end


// Stores parameters passed from JavaScript for deferred processing.
@interface CRWJSInvokeParameterQueue : NSObject

// YES if there are no more queued messages.
@property(nonatomic, readonly) BOOL isEmpty;

// Adds a new item to the queue. |commandString| is the escaped command string,
// |userIsInteracting| is true if the user was interacting with the page,
// |originURL| is the URL the command came from, and |windowId| is the id of the
// window that sent the command.
- (void)addCommandString:(NSString*)commandString
       userIsInteracting:(BOOL)userIsInteracting
               originURL:(const GURL&)originURL
             forWindowId:(NSString*)windowId;

// Removes from |queue_| any CRWJSInvokeParameters whose command string contains
// |commandString|.
- (void)removeCommandString:(NSString*)commandString;

// Removes the oldest item from the queue and returns it.
- (CRWJSInvokeParameters*)popInvokeParameters;

@end

@interface CRWJSInvokeParameterQueue (Testing)
// The number of items in the queue.
@property(nonatomic, readonly) NSUInteger queueLength;
@end

#endif  // IOS_WEB_WEB_STATE_JS_CRW_JS_INVOKE_PARAMETER_QUEUE_H_
