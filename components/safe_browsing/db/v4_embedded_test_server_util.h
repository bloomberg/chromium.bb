// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_EMBEDDED_TEST_SERVER_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_EMBEDDED_TEST_SERVER_UTIL_H_

#include <map>

#include "components/safe_browsing/db/safebrowsing.pb.h"
#include "url/gurl.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
}  // namespace net

namespace safe_browsing {

// This method does two things:
// 1. Rewrites the global V4 server URL prefix to point to the test server.
// 2. Registers the FullHash request handler with the server.
void StartRedirectingV4RequestsForTesting(
    const std::map<GURL, ThreatMatch>& response_map,
    net::test_server::EmbeddedTestServer* embedded_test_server);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_EMBEDDED_TEST_SERVER_UTIL_H_
