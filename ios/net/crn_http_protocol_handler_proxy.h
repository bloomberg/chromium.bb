// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_PROXY_H_
#define IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_PROXY_H_

#import <Foundation/Foundation.h>

#import "ios/net/clients/crn_network_client_protocol.h"

// HttpProtocolHandlerProxy is responsible for managing access to the
// NSURLProtocol and its client.
@protocol CRNHTTPProtocolHandlerProxy<CRNNetworkClientProtocol>

// Invalidates any reference to the protocol handler: the handler will never
// be called after this.
// Called from the client thread.
- (void)invalidate;

@end

#endif  // IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_PROXY_H_
