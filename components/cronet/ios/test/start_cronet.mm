// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>
#import <Foundation/Foundation.h>

#include "components/cronet/ios/test/start_cronet.h"

#include "base/at_exit.h"
#include "components/grpc_support/test/quic_test_server.h"

namespace cronet {

base::AtExitManager* g_at_exit_ = nullptr;

void StartCronetIfNecessary() {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    [Cronet setUserAgent:@"CronetTest/1.0.0.0" partial:NO];
    [Cronet setHttp2Enabled:true];
    [Cronet setQuicEnabled:true];
    [Cronet setSslKeyLogFileName:@"SSLKEYLOGFILE"];
    [Cronet addQuicHint:@"test.example.com"
                   port:grpc_support::kTestServerPort
                altPort:grpc_support::kTestServerPort];
    [Cronet enableTestCertVerifierForTesting];
    [Cronet
        setHostResolverRulesForTesting:@"MAP test.example.com 127.0.0.1,"
                                        "MAP notfound.example.com ~NOTFOUND"];
    [Cronet start];

    // This method must be called once from the main thread.
    if (!g_at_exit_)
      g_at_exit_ = new base::AtExitManager;
  }
}

}  // namespace cronet
