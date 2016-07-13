// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MAC_APP_OMAHACOMMUNICATION_H_
#define CHROME_INSTALLER_MAC_APP_OMAHACOMMUNICATION_H_

#import <Foundation/Foundation.h>

#include "NetworkCommunication.h"

typedef void (^OmahaRequestCompletionHandler)(NSData*, NSError*);

@interface OmahaCommunication : NSObject

@property(nonatomic, copy) NSXMLDocument* requestXMLBody;
@property(nonatomic, copy) NetworkCommunication* sessionHelper;

- (id)init;
- (id)initWithBody:(NSXMLDocument*)xmlBody;

// Sends the request created using the session helper.
- (void)sendRequestWithBlock:(OmahaRequestCompletionHandler)block;

@end

#endif  // CHROME_INSTALLER_MAC_APP_OMAHACOMMUNICATION_H_
