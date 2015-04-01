// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/clients/crw_passkit_network_client.h"

#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/web/net/clients/crw_passkit_delegate.h"
#include "ios/web/net/request_tracker_impl.h"
#include "net/base/net_errors.h"

@implementation CRWPassKitNetworkClient {
  id<CRWPassKitDelegate> _delegate;
  // Cache the PassKit object until data has been completely passed in.
  base::scoped_nsobject<NSMutableData> _passKitData;
  base::WeakPtr<net::RequestTracker> _tracker;
}

- (instancetype)initWithTracker:(base::WeakPtr<net::RequestTracker>)tracker
                       delegate:(id<CRWPassKitDelegate>)delegate {
  if ((self = [super init])) {
    DCHECK(delegate);
    _delegate = delegate;
    _tracker = tracker;
  }
  return self;
}

#pragma mark - CRNNetworkClientProtocol

- (void)didFailWithNSErrorCode:(NSInteger)nsErrorCode
                  netErrorCode:(int)netErrorCode {
  // If we fail, finish loading and return a nil PassKit object.
  _passKitData.reset();
  [self didFinishLoading];
}

- (void)didLoadData:(NSData*)data {
  [_passKitData appendData:data];
}

- (void)didReceiveResponse:(NSURLResponse*)response {
  _passKitData.reset([[NSMutableData alloc] init]);
}

- (void)wasRedirectedToRequest:(NSURLRequest*)request
                 nativeRequest:(net::URLRequest*)nativeRequest
              redirectResponse:(NSURLResponse*)redirectResponse {
  NOTREACHED();
}

- (void)didFinishLoading {
  // Tell |_delegate| that the PassKit object has loaded.
  base::scoped_nsobject<NSData> passKitData([_passKitData copy]);
  if (_tracker) {
    web::RequestTrackerImpl::PostUITaskIfOpen(
        _tracker,
        base::BindBlock(^{ [_delegate handlePassKitObject:passKitData]; }));
  }

  [super didFailWithNSErrorCode:NSURLErrorCancelled
                   netErrorCode:net::ERR_ABORTED];
}

- (void)setUserName:(NSString*)name password:(NSString*)password {
  NOTREACHED();
}

- (void)cancelAuthentication {
  NOTREACHED();
}

@end
