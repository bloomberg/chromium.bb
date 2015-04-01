// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CLIENTS_CRW_PASSKIT_DELEGATE_H_
#define IOS_WEB_NET_CLIENTS_CRW_PASSKIT_DELEGATE_H_

@protocol CRWPassKitDelegate<NSObject>

// Called when a PassKit object has been collected from the network.
- (void)handlePassKitObject:(NSData*)data;

@end

#endif  // IOS_WEB_NET_CLIENTS_CRW_PASSKIT_DELEGATE_H_
