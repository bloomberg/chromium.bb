// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_IOS_CRONET_METRICS_H_
#define COMPONENTS_CRONET_IOS_CRONET_METRICS_H_

#import <Foundation/Foundation.h>

#include "components/grpc_support/include/bidirectional_stream_c.h"

FOUNDATION_EXPORT GRPC_SUPPORT_EXPORT NS_AVAILABLE_IOS(10.0)
@interface CronetMetrics : NSURLSessionTaskTransactionMetrics

@property(copy, readwrite) NSURLRequest* request;
@property(copy, readwrite) NSURLResponse* response;

@property(copy, readwrite) NSDate* fetchStartDate;
@property(copy, readwrite) NSDate* domainLookupStartDate;
@property(copy, readwrite) NSDate* domainLookupEndDate;
@property(copy, readwrite) NSDate* connectStartDate;
@property(copy, readwrite) NSDate* secureConnectionStartDate;
@property(copy, readwrite) NSDate* secureConnectionEndDate;
@property(copy, readwrite) NSDate* connectEndDate;
@property(copy, readwrite) NSDate* requestStartDate;
@property(copy, readwrite) NSDate* requestEndDate;
@property(copy, readwrite) NSDate* responseStartDate;
@property(copy, readwrite) NSDate* responseEndDate;

@property(copy, readwrite) NSString* networkProtocolName;
@property(assign, readwrite, getter=isProxyConnection) BOOL proxyConnection;
@property(assign, readwrite, getter=isReusedConnection) BOOL reusedConnection;
@property(assign, readwrite)
    NSURLSessionTaskMetricsResourceFetchType resourceFetchType;

@end

#endif // COMPONENTS_CRONET_IOS_CRONET_METRICS_H_
