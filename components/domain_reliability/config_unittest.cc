// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/config.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

namespace {

scoped_ptr<DomainReliabilityConfig> MakeValidConfig() {
  DomainReliabilityConfig* config = new DomainReliabilityConfig();
  config->domain = "example";
  config->valid_until = 1234567890.0;
  config->version = "1";

  DomainReliabilityConfig::Resource* resource =
      new DomainReliabilityConfig::Resource();
  resource->name = "home";
  resource->url_patterns.push_back(
      new std::string("http://example/"));
  resource->success_sample_rate = 0.0;
  resource->failure_sample_rate = 1.0;
  config->resources.push_back(resource);

  resource = new DomainReliabilityConfig::Resource();
  resource->name = "static";
  resource->url_patterns.push_back(new std::string("http://example/css/*"));
  resource->url_patterns.push_back(new std::string("http://example/js/*"));
  resource->success_sample_rate = 0.0;
  resource->failure_sample_rate = 1.0;
  config->resources.push_back(resource);

  resource = new DomainReliabilityConfig::Resource();
  resource->name = "html";
  resource->url_patterns.push_back(
      new std::string("http://example/*.html"));
  resource->success_sample_rate = 0.0;
  resource->failure_sample_rate = 1.0;
  config->resources.push_back(resource);

  DomainReliabilityConfig::Collector* collector =
      new DomainReliabilityConfig::Collector();
  collector->upload_url = GURL("https://example/upload");
  config->collectors.push_back(collector);

  EXPECT_TRUE(config->IsValid());
  return scoped_ptr<DomainReliabilityConfig>(config);
}

int GetIndex(DomainReliabilityConfig* config, const char* url_string) {
  return config->GetResourceIndexForUrl(GURL(url_string));
}

}  // namespace

class DomainReliabilityConfigTest : public testing::Test { };

TEST_F(DomainReliabilityConfigTest, IsValid) {
  scoped_ptr<DomainReliabilityConfig> config;

  config = MakeValidConfig();
  EXPECT_TRUE(config->IsValid());

  config = MakeValidConfig();
  config->domain = "";
  EXPECT_FALSE(config->IsValid());

  config = MakeValidConfig();
  config->valid_until = 0.0;
  EXPECT_FALSE(config->IsValid());

  config = MakeValidConfig();
  config->version = "";
  EXPECT_FALSE(config->IsValid());

  config = MakeValidConfig();
  config->resources.clear();
  EXPECT_FALSE(config->IsValid());

  config = MakeValidConfig();
  config->resources[0]->name.clear();
  EXPECT_FALSE(config->IsValid());

  config = MakeValidConfig();
  config->resources[0]->url_patterns.clear();
  EXPECT_FALSE(config->IsValid());

  config = MakeValidConfig();
  config->resources[0]->success_sample_rate = 2.0;
  EXPECT_FALSE(config->IsValid());

  config = MakeValidConfig();
  config->resources[0]->failure_sample_rate = 2.0;
  EXPECT_FALSE(config->IsValid());

  config = MakeValidConfig();
  config->collectors.clear();
  EXPECT_FALSE(config->IsValid());

  config = MakeValidConfig();
  config->collectors[0]->upload_url = GURL();
  EXPECT_FALSE(config->IsValid());
}

TEST_F(DomainReliabilityConfigTest, IsExpired) {
  base::Time now = base::Time::Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  DomainReliabilityConfig unexpired_config;
  unexpired_config.valid_until = (now + one_day).ToDoubleT();
  EXPECT_FALSE(unexpired_config.IsExpired(now));

  DomainReliabilityConfig expired_config;
  expired_config.valid_until = (now - one_day).ToDoubleT();
  EXPECT_TRUE(expired_config.IsExpired(now));
}

TEST_F(DomainReliabilityConfigTest, GetResourceIndexForUrl) {
  scoped_ptr<DomainReliabilityConfig> config = MakeValidConfig();

  EXPECT_EQ(0, GetIndex(&*config, "http://example/"));
  EXPECT_EQ(1, GetIndex(&*config, "http://example/css/foo.css"));
  EXPECT_EQ(1, GetIndex(&*config, "http://example/js/bar.js"));
  EXPECT_EQ(2, GetIndex(&*config, "http://example/test.html"));
  EXPECT_EQ(-1, GetIndex(&*config, "http://example/no-resource"));
}

TEST_F(DomainReliabilityConfigTest, FromJSON) {
  std::string config_json =
    "{ \"config_version\": \"1\","
    "  \"config_valid_until\": 1234567890.0,"
    "  \"monitored_domain\": \"test.example\","
    "  \"monitored_resources\": [ {"
    "    \"resource_name\": \"home\","
    "    \"url_patterns\": [ \"http://test.example/\" ],"
    "    \"success_sample_rate\": 0.01,"
    "    \"failure_sample_rate\": 0.10"
    "  } ],"
    "  \"collectors\": [ {"
    "    \"upload_url\": \"https://test.example/domrel/upload\""
    "  } ]"
    "}";

  scoped_ptr<const DomainReliabilityConfig> config(
      DomainReliabilityConfig::FromJSON(config_json));

  EXPECT_TRUE(config);
  EXPECT_EQ("1", config->version);
  EXPECT_EQ(1234567890.0, config->valid_until);
  EXPECT_EQ("test.example", config->domain);
  EXPECT_EQ(1u, config->resources.size());
  EXPECT_EQ("home", config->resources[0]->name);
  EXPECT_EQ(1u, config->resources[0]->url_patterns.size());
  EXPECT_EQ("http://test.example/", *(config->resources[0]->url_patterns[0]));
  EXPECT_EQ(0.01, config->resources[0]->success_sample_rate);
  EXPECT_EQ(0.10, config->resources[0]->failure_sample_rate);
  EXPECT_EQ(1u, config->collectors.size());
  EXPECT_EQ(GURL("https://test.example/domrel/upload"),
      config->collectors[0]->upload_url);
}

}  // namespace domain_reliability
