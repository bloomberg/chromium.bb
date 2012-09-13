// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for implementation of google_api_keys namespace.
//
// Because the file deals with a lot of preprocessor defines and
// optionally includes an internal header, the way we test is by
// including the .cc file multiple times with different defines set.
// This is a little unorthodox, but it lets us test the behavior as
// close to unmodified as possible.

#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(GOOGLE_CHROME_BUILD) or defined(USE_OFFICIAL_GOOGLE_API_KEYS)
// Test official build behavior, since we are in a checkout where this
// is possible.
namespace official_build {

#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_CLOUD_PRINT
#undef GOOGLE_CLIENT_SECRET_CLOUD_PRINT
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING

// Try setting some keys, these should be ignored since it's a build
// with official keys.
#define GOOGLE_API_KEY "bogus api key"
#define GOOGLE_CLIENT_ID_MAIN "bogus client_id_main"

#include "google_apis/google_api_keys.cc"

TEST(GoogleAPIKeys, OfficialKeys) {
  std::string api_key = g_api_key_cache.Get().api_key();
  std::string id_main = g_api_key_cache.Get().GetClientID(CLIENT_MAIN);
  std::string secret_main = g_api_key_cache.Get().GetClientSecret(CLIENT_MAIN);
  std::string id_cloud_print =
      g_api_key_cache.Get().GetClientID(CLIENT_CLOUD_PRINT);
  std::string secret_cloud_print =
      g_api_key_cache.Get().GetClientSecret(CLIENT_CLOUD_PRINT);
  std::string id_remoting = g_api_key_cache.Get().GetClientID(CLIENT_REMOTING);
  std::string secret_remoting =
      g_api_key_cache.Get().GetClientSecret(CLIENT_REMOTING);

  ASSERT_TRUE(api_key.size() == 0);
}

}  // namespace official_build
#endif  // defined(GOOGLE_CHROME_BUILD) or defined(USE_OFFICIAL_GOOGLE_API_KEYS)


}  // namespace
