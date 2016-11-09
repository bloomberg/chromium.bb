// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>
#import <Foundation/Foundation.h>

#include "components/cronet/ios/test/start_cronet.h"
#include "components/grpc_support/test/get_stream_engine.h"

namespace grpc_support {

stream_engine* GetTestStreamEngine(int port) {
  cronet::StartCronetIfNecessary(port);
  return [Cronet getGlobalEngine];
}

}  // namespace grpc_support
