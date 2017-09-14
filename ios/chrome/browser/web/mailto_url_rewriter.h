// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MAILTO_URL_REWRITER_H_
#define IOS_CHROME_BROWSER_WEB_MAILTO_URL_REWRITER_H_

#import <Foundation/Foundation.h>

@class MailtoHandler;
@class MailtoURLRewriter;
class GURL;

// Protocol that must be implemented by observers of MailtoURLRewriter object.
@protocol MailtoURLRewriterObserver<NSObject>
// The default mailto: handler has been changed.
- (void)rewriterDidChange:(MailtoURLRewriter*)rewriter;
@end

// An abstract base class for objects that manage the available Mail client
// apps. The currently selected Mail client to handle mailto: URL is available
// through -defaultHandlerID property. If the corresponding app is no longer
// installed, the system-provided Mail client app will be used.
@interface MailtoURLRewriter : NSObject

// The unique ID of the Mail client app that handles mailto: URL scheme.
// This has a value of nil if default has not been set.
@property(nonatomic, copy) NSString* defaultHandlerID;

// Array of all the currently supported Mail client apps that claim to handle
// mailto: URL scheme through their own custom defined URL schemes.
@property(nonatomic, strong) NSArray<MailtoHandler*>* defaultHandlers;

// Observer object that will be called when |defaultHandlerID| is changed.
@property(nonatomic, weak) id<MailtoURLRewriterObserver> observer;

// Returns the NSString* key to store state in NSUserDefaults.
+ (NSString*)userDefaultsKey;

// Returns the ID as a string for the system-provided Mail client app.
+ (NSString*)systemMailApp;

// Convenience method to return a new instance of this class initialized with
// a standard set of MailtoHandlers.
+ (instancetype)mailtoURLRewriterWithStandardHandlers;

// Returns the name of the application that handles mailto: URLs. Returns nil
// if a default has not been set.
- (NSString*)defaultHandlerName;

// Returns the mailto:// handler app corresponding to |handlerID|. Returns nil
// if there is no handler corresponding to |handlerID|.
- (MailtoHandler*)defaultHandlerByID:(NSString*)handlerID;

// Rewrites |URL| into a new URL that can be "opened" to launch the Mail
// client app. May return nil if |URL| is not a mailto: URL, a mail client
// app has not been selected, or there are no Mail client app available.
- (NSString*)rewriteMailtoURL:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MAILTO_URL_REWRITER_H_
