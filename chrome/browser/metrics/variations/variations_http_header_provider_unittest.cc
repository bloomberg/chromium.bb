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
                         std::set<VariationID>* variation_ids,
                         std::set<VariationID>* trigger_ids) {
  std::string serialized_proto;
  if (!base::Base64Decode(variations, &serialized_proto))
    return false;
  metrics::ChromeVariations proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;
  for (int i = 0; i < proto.variation_id_size(); ++i)
    variation_ids->insert(proto.variation_id(i));
  for (int i = 0; i < proto.trigger_variation_id_size(); ++i)
    trigger_ids->insert(proto.trigger_variation_id(i));
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
    { "http://google", false },

    { "http://youtube.com", true },
    { "http://www.youtube.com", true },
    { "http://www.youtube.ca", true },
    { "http://www.youtube.co.uk:8080/", true },
    { "https://www.youtube.com", true },
    { "http://youtube", false },

    { "http://www.yahoo.com", false },

    { "http://ad.doubleclick.net", true },
    { "https://a.b.c.doubleclick.net", true },
    { "https://a.b.c.doubleclick.net:8081", true },
    { "http://www.doubleclick.com", false },
    { "http://www.doubleclick.net.com", false },
    { "https://www.doubleclick.net.com", false },

    { "http://ad.googlesyndication.com", true },
    { "https://a.b.c.googlesyndication.com", true },
    { "https://a.b.c.googlesyndication.com:8080", true },
    { "http://www.doubleclick.edu", false },
    { "http://www.googlesyndication.com.edu", false },
    { "https://www.googlesyndication.com.com", false },

    { "http://www.googleadservices.com", true },
    { "http://www.googleadservices.com:8080", true },
    { "https://www.googleadservices.com", true },
    { "https://www.internal.googleadservices.com", false },
    { "https://www2.googleadservices.com", false },
    { "https://www.googleadservices.org", false },
    { "https://www.googleadservices.com.co.uk", false },
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
  EXPECT_TRUE(provider.SetDefaultVariationIds("12,456,t789"));
  provider.InitVariationIDsCacheIfNeeded();
  provider.AppendHeaders(url, false, false, &headers);
  EXPECT_TRUE(headers.HasHeader("X-Client-Data"));
  headers.GetHeader("X-Client-Data", &variations);
  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
  EXPECT_TRUE(variation_ids.find(12) != variation_ids.end());
  EXPECT_TRUE(variation_ids.find(456) != variation_ids.end());
  EXPECT_TRUE(trigger_ids.find(789) != trigger_ids.end());
  EXPECT_FALSE(variation_ids.find(789) != variation_ids.end());
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
  EXPECT_FALSE(headers.HasHeader("X-Client-Data"));

  // Invalid trigger experiment id
  EXPECT_FALSE(provider.SetDefaultVariationIds("12,tabc456"));
  provider.InitVariationIDsCacheIfNeeded();
  provider.AppendHeaders(url, false, false, &headers);
  EXPECT_FALSE(headers.HasHeader("X-Client-Data"));
}

}  // namespace chrome_variations
