// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_TEST_SERVER_LOCATIONS_H_
#define CHROME_COMMON_NET_TEST_SERVER_LOCATIONS_H_
#pragma once

namespace chrome_common_net {

// Hostname used for the NetworkStats test. Should point to a TCP/UDP server
// that simply echoes back the request.
extern const char kEchoTestServerLocation[];

// Base URL for the HttpPipeliningCompatibilityClient test. Should point to a
// server running server.py from http://code.google.com/p/http-pipelining-test/
extern const char kPipelineTestServerBaseUrl[];

}  // namespace chrome_common_net

#endif  // CHROME_COMMON_NET_TEST_SERVER_LOCATIONS_H_
