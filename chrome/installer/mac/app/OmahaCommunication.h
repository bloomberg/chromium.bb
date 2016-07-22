// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MAC_APP_OMAHACOMMUNICATION_H_
#define CHROME_INSTALLER_MAC_APP_OMAHACOMMUNICATION_H_

#import <Foundation/Foundation.h>

#import "NetworkCommunication.h"

@protocol OmahaCommunicationDelegate
- (void)onOmahaSuccessWithResponseBody:(NSData*)responseBody
                              AndError:(NSError*)error;
@end

@interface OmahaCommunication : NSObject<NSURLSessionDataDelegate> {
  id<OmahaCommunicationDelegate> _delegate;
}

@property(nonatomic, copy) NSXMLDocument* requestXMLBody;
// TODO: talk to @sdy about use of NetworkCommunication
@property(nonatomic, copy) NetworkCommunication* sessionHelper;
@property(nonatomic, assign) id<OmahaCommunicationDelegate> delegate;

- (id)init;
- (id)initWithBody:(NSXMLDocument*)xmlBody;

// Sends the request created using the session helper.
- (void)sendRequest;

@end

#endif  // CHROME_INSTALLER_MAC_APP_OMAHACOMMUNICATION_H_
