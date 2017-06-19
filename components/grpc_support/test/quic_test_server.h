// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GRPC_SUPPORT_TEST_QUIC_TEST_SERVER_H_
#define COMPONENTS_GRPC_SUPPORT_TEST_QUIC_TEST_SERVER_H_

#include <string>

namespace grpc_support {

bool StartQuicTestServer();

void ShutdownQuicTestServer();

int GetQuicTestServerPort();

extern const char kTestServerDomain[];
extern const char kTestServerHost[];
extern const char kTestServerUrl[];

extern const char kStatusHeader[];

extern const char kHelloBodyValue[];
extern const char kHelloStatus[];

extern const char kHelloHeaderName[];
extern const char kHelloHeaderValue[];

extern const char kHelloTrailerName[];
extern const char kHelloTrailerValue[];

// Simple Url returns response without HTTP/2 trailers.
extern const char kTestServerSimpleUrl[];
extern const char kSimpleBodyValue[];
extern const char kSimpleStatus[];
extern const char kSimpleHeaderName[];
extern const char kSimpleHeaderValue[];

}  // namespace grpc_support

#endif  // COMPONENTS_CRONET_IOS_TEST_QUIC_TEST_SERVER_H_
