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

// An object that manages the available Mail client apps. The currently selected
// Mail client to handle mailto: URL is stored in a key in NSUserDefaults.
// If the key in NSUserDefaults is not set, the system-provided Mail client app
// will be used.
@interface MailtoURLRewriter : NSObject

// The unique ID of the Mail client app was set to handle mailto: URL scheme.
@property(nonatomic, copy) NSString* defaultHandlerID;

// Observer object that will be called when |defaultHandlerID| is changed.
@property(nonatomic, weak) id<MailtoURLRewriterObserver> observer;

// Returns the ID as a string for the system-provided Mail client app.
+ (NSString*)systemMailApp;

// An initializer returning an instance that has the standard set of
// MailtoHandlers initialized. Unit tests can use -init and then set up the
// different handlers.
- (instancetype)initWithStandardHandlers;

// Returns a sorted array of all the currently supported Mail client apps that
// claim to handle mailto: URL scheme through their own custom defined URL
// schemes.
- (NSArray<MailtoHandler*>*)defaultHandlers;

// Returns the name of the application that handles mailto: URLs.
- (NSString*)defaultHandlerName;

// Rewrites |gURL| into a new URL that can be "opened" to launch the Mail
// client app. May return nil if |gURL| is not a mailto: URL or there are no
// Mail client app available.
- (NSString*)rewriteMailtoURL:(const GURL&)gURL;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MAILTO_URL_REWRITER_H_
