// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBP_TRANSCODE_WEBP_NETWORK_CLIENT_H_
#define COMPONENTS_WEBP_TRANSCODE_WEBP_NETWORK_CLIENT_H_

#import <Foundation/Foundation.h>

#include "base/memory/ref_counted.h"
#import "ios/net/clients/crn_forwarding_network_client.h"
#import "ios/net/clients/crn_forwarding_network_client_factory.h"

// Network client that decodes WebP images.
@interface WebPNetworkClient : CRNForwardingNetworkClient

// |runner| is the task runner used to perform the image decoding.
// Designated initializer.
- (instancetype)initWithTaskRunner:
    (const scoped_refptr<base::SequencedTaskRunner>&)runner;

@end

#endif  // COMPONENTS_WEBP_TRANSCODE_WEBP_NETWORK_CLIENT_H_
