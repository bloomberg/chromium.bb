// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MAILTO_URL_REWRITER_H_
#define IOS_CHROME_BROWSER_WEB_MAILTO_URL_REWRITER_H_

#import <Foundation/Foundation.h>

@class MailtoHandler;
class GURL;

// An object that manages the available Mail client apps. The currently selected
// Mail client to handle mailto: URL is stored in a key in NSUserDefaults.
// If the key in NSUserDefaults is not set, the system-provided Mail client app
// will be used.
@interface MailtoURLRewriter : NSObject

// The unique ID of the Mail client app was set to handle mailto: URL scheme.
@property(nonatomic, copy) NSString* defaultHandlerID;

// Returns the ID as a string for the system-provided Mail client app.
+ (NSString*)systemMailApp;

// An initializer returning an instance that has the standard set of
// MailtoHandlers initialized. Unit tests can use -init and then set up the
// different handlers.
- (instancetype)initWithStandardHandlers;

// Returns an array of all the currently supported Mail client apps that claims
// to handle mailto: URL scheme through their own custom defined URL schemes.
- (NSArray<MailtoHandler*>*)defaultHandlers;

// Rewrites |gURL| into a new URL that can be "opened" to launch the Mail
// client app. May return nil if |gURL| is not a mailto: URL or there are no
// Mail client app available.
- (NSString*)rewriteMailtoURL:(const GURL&)gURL;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MAILTO_URL_REWRITER_H_
