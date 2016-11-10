// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GRPC_SUPPORT_TEST_GET_REQUEST_CONTEXT_GETTER_H_
#define COMPONENTS_GRPC_SUPPORT_TEST_GET_REQUEST_CONTEXT_GETTER_H_

struct stream_engine;

namespace grpc_support {

// Returns a stream_engine* for testing with the QuicTestServer.
// The engine returned should resolve kTestServerHost as localhost:|port|,
// and should have kTestServerHost configured as a QUIC server.
stream_engine* GetTestStreamEngine(int port);

}  // namespace grpc_support

#endif  // COMPONENTS_GRPC_SUPPORT_TEST_GET_REQUEST_CONTEXT_GETTER_H_
