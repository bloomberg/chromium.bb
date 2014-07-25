// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_http_header_provider.h"

#include <string>

#include "base/base64.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/variations_associated_data.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// TODO(mathp): Remove once everything is under namespace 'variations'.
using chrome_variations::GOOGLE_WEB_PROPERTIES;
using chrome_variations::GOOGLE_WEB_PROPERTIES_TRIGGER;
using chrome_variations::IDCollectionKey;
using chrome_variations::VariationID;

namespace variations {

namespace {

// Decodes the variations header and extracts the variation ids.
bool ExtractVariationIds(const std::string& variations,
                         std::set<VariationID>* variation_ids,
                         std::set<VariationID>* trigger_ids) {
  std::string serialized_proto;
  if (!base::Base64Decode(variations, &serialized_proto))
    return false;
  ClientVariations proto;
  if (!proto.ParseFromString(serialized_proto)) return false;
  for (int i = 0; i < proto.variation_id_size(); ++i)
    variation_ids->insert(proto.variation_id(i));
  for (int i = 0; i < proto.trigger_variation_id_size(); ++i)
    trigger_ids->insert(proto.trigger_variation_id(i));
  return true;
}

scoped_refptr<base::FieldTrial> CreateTrialAndAssociateId(
    const std::string& trial_name, const std::string& default_group_name,
    IDCollectionKey key, VariationID id) {
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::CreateFieldTrial(trial_name, default_group_name));

  chrome_variations::AssociateGoogleVariationID(key, trial->trial_name(),
                                                trial->group_name(), id);

  return trial;
}

}  // namespace

class VariationsHttpHeaderProviderTest : public ::testing::Test {
 public:
  VariationsHttpHeaderProviderTest() {}

  virtual ~VariationsHttpHeaderProviderTest() {}

  virtual void TearDown() OVERRIDE {
    chrome_variations::testing::ClearAllVariationIDs();
  }
};

TEST_F(VariationsHttpHeaderProviderTest, ShouldAppendHeaders) {
  struct {
    const char* url;
    bool should_append_headers;
  } cases[] = {
        {"http://google.com", true},
        {"http://www.google.com", true},
        {"http://m.google.com", true},
        {"http://google.ca", true},
        {"https://google.ca", true},
        {"http://google.co.uk", true},
        {"http://google.co.uk:8080/", true},
        {"http://www.google.co.uk:8080/", true},
        {"http://google", false},

        {"http://youtube.com", true},
        {"http://www.youtube.com", true},
        {"http://www.youtube.ca", true},
        {"http://www.youtube.co.uk:8080/", true},
        {"https://www.youtube.com", true},
        {"http://youtube", false},

        {"http://www.yahoo.com", false},

        {"http://ad.doubleclick.net", true},
        {"https://a.b.c.doubleclick.net", true},
        {"https://a.b.c.doubleclick.net:8081", true},
        {"http://www.doubleclick.com", true},
        {"http://www.doubleclick.org", false},
        {"http://www.doubleclick.net.com", false},
        {"https://www.doubleclick.net.com", false},

        {"http://ad.googlesyndication.com", true},
        {"https://a.b.c.googlesyndication.com", true},
        {"https://a.b.c.googlesyndication.com:8080", true},
        {"http://www.doubleclick.edu", false},
        {"http://www.googlesyndication.com.edu", false},
        {"https://www.googlesyndication.com.com", false},

        {"http://www.googleadservices.com", true},
        {"http://www.googleadservices.com:8080", true},
        {"https://www.googleadservices.com", true},
        {"https://www.internal.googleadservices.com", true},
        {"https://www2.googleadservices.com", true},
        {"https://www.googleadservices.org", false},
        {"https://www.googleadservices.com.co.uk", false},

        {"http://WWW.ANDROID.COM", true},
        {"http://www.android.com", true},
        {"http://www.doubleclick.com", true},
        {"http://www.doubleclick.net", true},
        {"http://www.ggpht.com", true},
        {"http://www.googleadservices.com", true},
        {"http://www.googleapis.com", true},
        {"http://www.googlesyndication.com", true},
        {"http://www.googleusercontent.com", true},
        {"http://www.googlevideo.com", true},
        {"http://ssl.gstatic.com", true},
        {"http://www.gstatic.com", true},
        {"http://www.ytimg.com", true},
        {"http://wwwytimg.com", false},
        {"http://ytimg.com", false},

        {"http://www.android.org", false},
        {"http://www.doubleclick.org", false},
        {"http://www.doubleclick.net", true},
        {"http://www.ggpht.org", false},
        {"http://www.googleadservices.org", false},
        {"http://www.googleapis.org", false},
        {"http://www.googlesyndication.org", false},
        {"http://www.googleusercontent.org", false},
        {"http://www.googlevideo.org", false},
        {"http://ssl.gstatic.org", false},
        {"http://www.gstatic.org", false},
        {"http://www.ytimg.org", false},

        {"http://a.b.android.com", true},
        {"http://a.b.doubleclick.com", true},
        {"http://a.b.doubleclick.net", true},
        {"http://a.b.ggpht.com", true},
        {"http://a.b.googleadservices.com", true},
        {"http://a.b.googleapis.com", true},
        {"http://a.b.googlesyndication.com", true},
        {"http://a.b.googleusercontent.com", true},
        {"http://a.b.googlevideo.com", true},
        {"http://ssl.gstatic.com", true},
        {"http://a.b.gstatic.com", true},
        {"http://a.b.ytimg.com", true},

    };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    const GURL url(cases[i].url);
    EXPECT_EQ(cases[i].should_append_headers,
              VariationsHttpHeaderProvider::ShouldAppendHeaders(url))
        << url;
  }
}

TEST_F(VariationsHttpHeaderProviderTest, SetDefaultVariationIds_Valid) {
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

TEST_F(VariationsHttpHeaderProviderTest, SetDefaultVariationIds_Invalid) {
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

TEST_F(VariationsHttpHeaderProviderTest, OnFieldTrialGroupFinalized) {
  base::MessageLoop loop;
  base::FieldTrialList field_trial_list(
      new metrics::SHA1EntropyProvider("test"));
  VariationsHttpHeaderProvider provider;
  provider.InitVariationIDsCacheIfNeeded();

  const std::string default_name = "default";
  scoped_refptr<base::FieldTrial> trial_1(CreateTrialAndAssociateId(
      "t1", default_name, GOOGLE_WEB_PROPERTIES, 123));

  ASSERT_EQ(default_name, trial_1->group_name());

  scoped_refptr<base::FieldTrial> trial_2(CreateTrialAndAssociateId(
      "t2", default_name, GOOGLE_WEB_PROPERTIES_TRIGGER, 456));

  ASSERT_EQ(default_name, trial_2->group_name());

  // Run the message loop to make sure OnFieldTrialGroupFinalized is called for
  // the two field trials.
  base::RunLoop().RunUntilIdle();

  GURL url("http://www.google.com");
  net::HttpRequestHeaders headers;
  provider.AppendHeaders(url, false, false, &headers);
  std::string variations;
  headers.GetHeader("X-Client-Data", &variations);

  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
  EXPECT_TRUE(variation_ids.find(123) != variation_ids.end());
  EXPECT_TRUE(trigger_ids.find(456) != trigger_ids.end());
}

}  // namespace variations
