// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MAC_APP_OMAHACOMMUNICATION_H_
#define CHROME_INSTALLER_MAC_APP_OMAHACOMMUNICATION_H_

#import <Foundation/Foundation.h>

@protocol OmahaCommunicationDelegate
- (void)onOmahaSuccessWithURLs:(NSArray*)URLs;
- (void)onOmahaFailureWithError:(NSError*)error;
@end

@interface OmahaCommunication : NSObject<NSURLSessionDataDelegate>

@property(nonatomic, copy) NSXMLDocument* requestXMLBody;
@property(nonatomic, assign) id<OmahaCommunicationDelegate> delegate;

- (id)init;
- (id)initWithBody:(NSXMLDocument*)xmlBody;

// Asks the Omaha servers for the most updated version of Chrome by sending a
// request using this function.
- (void)fetchDownloadURLs;

@end

#endif  // CHROME_INSTALLER_MAC_APP_OMAHACOMMUNICATION_H_
