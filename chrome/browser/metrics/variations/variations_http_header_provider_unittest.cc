// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_http_header_provider.h"

#include <string>

#include "base/base64.h"
#include "base/message_loop/message_loop.h"
#include "chrome/common/metrics/proto/chrome_experiments.pb.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome_variations {

namespace {

// Decodes the variations header and extracts the variation ids.
bool ExtractVariationIds(const std::string& variations,
                       std::set<VariationID>* variation_ids) {
  std::string serialized_proto;
  if (!base::Base64Decode(variations, &serialized_proto))
    return false;
  metrics::ChromeVariations proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;
  for (int i = 0; i < proto.variation_id_size(); ++i)
    variation_ids->insert(proto.variation_id(i));
  return true;
}

}  // namespace

TEST(VariationsHttpHeaderProviderTest, ShouldAppendHeaders) {
  struct {
    const char* url;
    bool should_append_headers;
  } cases[] = {
    { "http://google.com", true },
    { "http://www.google.com", true },
    { "http://m.google.com", true },
    { "http://google.ca", true },
    { "https://google.ca", true },
    { "http://google.co.uk", true },
    { "http://google.co.uk:8080/", true },
    { "http://www.google.co.uk:8080/", true },
    { "http://youtube.com", true },
    { "http://www.youtube.com", true },
    { "http://www.youtube.ca", true },
    { "http://www.youtube.co.uk:8080/", true },
    { "https://www.youtube.com", true },
    { "http://www.yahoo.com", false },
    { "http://youtube", false },
    { "http://google", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    const GURL url(cases[i].url);
    EXPECT_EQ(cases[i].should_append_headers,
              VariationsHttpHeaderProvider::ShouldAppendHeaders(url)) << url;
  }
}

TEST(VariationsHttpHeaderProviderTest, SetDefaultVariationIds_Valid) {
  base::MessageLoop loop;
  VariationsHttpHeaderProvider provider;
  GURL url("http://www.google.com");
  net::HttpRequestHeaders headers;
  std::string variations;

  // Valid experiment ids.
  EXPECT_TRUE(provider.SetDefaultVariationIds("12,456"));
  provider.InitVariationIDsCacheIfNeeded();
  provider.AppendHeaders(url, false, false, &headers);
  EXPECT_TRUE(headers.HasHeader("X-Chrome-Variations"));
  headers.GetHeader("X-Chrome-Variations", &variations);
  std::set<VariationID> variation_ids;
  ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids));
  EXPECT_TRUE(variation_ids.find(12) != variation_ids.end());
  EXPECT_TRUE(variation_ids.find(456) != variation_ids.end());
}

TEST(VariationsHttpHeaderProviderTest, SetDefaultVariationIds_Invalid) {
  base::MessageLoop loop;
  VariationsHttpHeaderProvider provider;
  GURL url("http://www.google.com");
  net::HttpRequestHeaders headers;

  // Invalid experiment ids.
  EXPECT_FALSE(provider.SetDefaultVariationIds("abcd12,456"));
  provider.InitVariationIDsCacheIfNeeded();
  provider.AppendHeaders(url, false, false, &headers);
  EXPECT_FALSE(headers.HasHeader("X-Chrome-Variations"));
}

}  // namespace chrome_variations
