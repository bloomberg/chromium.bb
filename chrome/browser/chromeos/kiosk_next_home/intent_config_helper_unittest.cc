// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/intent_config_helper.h"

#include <memory>

#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {
namespace kiosk_next_home {

namespace {

const char* kTestJsonConfig = R"(
{
  "allowed_hosts": ["example.com", "test.test.com"],
  "allowed_custom_schemes": [
    {
      "scheme": "example",
      "path": "//path"
    },
    {
      "scheme": "test",
      "path": "//something"
    }
  ]
})";

const char* kHostsOnlyTestJsonConfig = R"(
{ "allowed_hosts": ["example.com", "test.test.com"] })";

// Quotation mark intentionally missing below.
const char* kTestInvalidJsonConfig = R"(
{ "allowed_hosts": ["example.com", "test.test.com] })";

}  // namespace

class FakeConfigDelegate : public IntentConfigHelper::Delegate {
 public:
  explicit FakeConfigDelegate(const char* json_config)
      : json_config_(json_config) {}

  // IntentConfigHelper::Delegate:
  std::string GetJsonConfig() const override { return json_config_; }

 private:
  std::string json_config_;
};

class IntentConfigHelperTest : public testing::Test {
 protected:
  std::unique_ptr<IntentConfigHelper> intent_config_helper(
      const char* json_config) {
    auto intent_config_helper = std::make_unique<IntentConfigHelper>(
        std::make_unique<FakeConfigDelegate>(json_config));
    // Ensure config is read and parsed.
    test_thread_bundle_.RunUntilIdle();
    return intent_config_helper;
  }

 private:
  content::TestBrowserThreadBundle test_thread_bundle_;
  data_decoder::TestingJsonParser::ScopedFactoryOverride factory_override_;
  content::TestServiceManagerContext service_manager_context_;
};

TEST_F(IntentConfigHelperTest, IntentNotAllowedWithInvalidUri) {
  auto intent_helper = intent_config_helper(kTestJsonConfig);
  EXPECT_FALSE(intent_helper->IsIntentAllowed(GURL("invaliduri")));
}

TEST_F(IntentConfigHelperTest, IntentAllowedForAllowedHosts) {
  auto intent_helper = intent_config_helper(kTestJsonConfig);
  EXPECT_TRUE(intent_helper->IsIntentAllowed(GURL("https://example.com/")));
  EXPECT_TRUE(intent_helper->IsIntentAllowed(
      GURL("https://example.com/path?query=a#ref")));
  EXPECT_TRUE(intent_helper->IsIntentAllowed(GURL("https://test.test.com/")));
}

TEST_F(IntentConfigHelperTest, IntentNotAllowedForHostsNotInConfig) {
  auto intent_helper = intent_config_helper(kTestJsonConfig);
  EXPECT_FALSE(intent_helper->IsIntentAllowed(GURL("https://test.com")));
  EXPECT_FALSE(
      intent_helper->IsIntentAllowed(GURL("https://test.example.com")));
}

TEST_F(IntentConfigHelperTest, IntentAllowedForAllowedHostsOnly) {
  auto intent_helper = intent_config_helper(kHostsOnlyTestJsonConfig);
  EXPECT_TRUE(intent_helper->IsIntentAllowed(GURL("https://example.com/")));
  EXPECT_TRUE(intent_helper->IsIntentAllowed(
      GURL("https://example.com/path?query=a#ref")));
  EXPECT_TRUE(intent_helper->IsIntentAllowed(GURL("https://test.test.com/")));
  // No custom-scheme intents are allowed.
  EXPECT_FALSE(
      intent_helper->IsIntentAllowed(GURL("example.com://path?id=test")));
  EXPECT_FALSE(intent_helper->IsIntentAllowed(GURL("test://something?q=test")));
}

TEST_F(IntentConfigHelperTest, HttpIntentNotAllowed) {
  auto intent_helper = intent_config_helper(kTestJsonConfig);
  EXPECT_FALSE(intent_helper->IsIntentAllowed(GURL("http://example.com")));
}

TEST_F(IntentConfigHelperTest, IntentAllowedForAllowedCustomSchemes) {
  auto intent_helper = intent_config_helper(kTestJsonConfig);
  EXPECT_TRUE(intent_helper->IsIntentAllowed(GURL("example://path?id=test")));
  EXPECT_TRUE(intent_helper->IsIntentAllowed(GURL("test://something?q=test")));
}

TEST_F(IntentConfigHelperTest, IntentNotAllowedForCustomSchemesNotInConfig) {
  auto intent_helper = intent_config_helper(kTestJsonConfig);
  EXPECT_FALSE(
      intent_helper->IsIntentAllowed(GURL("example://different_path?id=test")));
  EXPECT_FALSE(intent_helper->IsIntentAllowed(GURL("scheme://path")));
}

TEST_F(IntentConfigHelperTest, IntentNotAllowedWhenConfigIsInvalid) {
  auto intent_helper = intent_config_helper(kTestInvalidJsonConfig);
  // With an invalid config, no intents are allowed.
  EXPECT_FALSE(intent_helper->IsIntentAllowed(GURL("https://example.com/")));
  EXPECT_FALSE(intent_helper->IsIntentAllowed(GURL("example://path?id=test")));
}

TEST_F(IntentConfigHelperTest, IntentNotAllowedWhenConfigIsEmpty) {
  auto intent_helper = intent_config_helper("{}");
  // With an empty config, no intents are allowed.
  EXPECT_FALSE(intent_helper->IsIntentAllowed(GURL("https://example.com/")));
  EXPECT_FALSE(intent_helper->IsIntentAllowed(GURL("example://path?id=test")));
}

}  // namespace kiosk_next_home
}  // namespace chromeos
