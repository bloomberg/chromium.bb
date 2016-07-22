// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MAC_APP_NETWORKCOMMUNICATION_H_
#define CHROME_INSTALLER_MAC_APP_NETWORKCOMMUNICATION_H_

#import <Foundation/Foundation.h>

// TODO: talk to @sdy about this class
@interface NetworkCommunication : NSObject

typedef void (^DataTaskCompletionHandler)(NSData*, NSURLResponse*, NSError*);

@property(nonatomic, copy) NSMutableURLRequest* request;
@property(nonatomic, copy) NSURLSession* session;
@property(nonatomic, copy) DataTaskCompletionHandler dataResponseHandler;

- (id)init;
- (id)initWithDelegate:(id)delegate;

// Creates a mutable URLRequest object as an instance variable, then returns it
// so the caller has a chance to edit it before sending the request out.
- (NSMutableURLRequest*)createRequestWithUrlAsString:(NSString*)urlString
                                          andXMLBody:(NSXMLDocument*)body;
// Adds a data task to the run loop using the request instance variable.
- (void)sendDataRequest;
// Adds a download task to the run loop using the request instance variable.
- (void)sendDownloadRequest;

@end

#endif  // CHROME_INSTALLER_MAC_APP_NETWORKCOMMUNICATION_H_
