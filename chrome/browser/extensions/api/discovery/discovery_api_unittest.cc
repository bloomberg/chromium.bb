// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chrome.alarms extension API.

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/discovery/discovery_api.h"
#include "chrome/browser/extensions/api/discovery/suggested_link.h"
#include "chrome/browser/extensions/api/discovery/suggested_links_registry.h"
#include "chrome/browser/extensions/api/discovery/suggested_links_registry_factory.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/experimental_discovery.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace utils = extension_function_test_utils;

namespace {
typedef extensions::SuggestedLinksRegistry::SuggestedLinkList SuggestedLinkList;
typedef extensions::api::experimental_discovery::SuggestDetails SuggestDetails;

// Converts suggest details used as parameter into a JSON string.
std::string SuggestDetailsParamToJSON(const SuggestDetails& suggest_details) {
  std::string result;
  scoped_ptr<base::DictionaryValue> value = suggest_details.ToValue();
  base::ListValue params;
  params.Append(value.release());  // |params| takes ownership of |value|.
  base::JSONWriter::Write(&params, &result);
  return result;
}

// Converts the parameters to the API Suggest method to JSON (without score).
std::string UnscoredSuggestParamsToJSON(const std::string& link_url,
                                        const std::string& link_text) {
  SuggestDetails suggest_details;
  suggest_details.link_url = link_url;
  suggest_details.link_text = link_text;
  return SuggestDetailsParamToJSON(suggest_details);
}


// Converts the parameters to the API Suggest method to JSON.
std::string SuggestParamsToJSON(const std::string& link_url,
                                const std::string& link_text,
                                double score) {
  SuggestDetails suggest_details;
  suggest_details.score.reset(new double(score));
  suggest_details.link_url = link_url;
  suggest_details.link_text = link_text;
  return SuggestDetailsParamToJSON(suggest_details);
}

// Converts the parameters to the API RemoveSuggestion method to JSON.
std::string RemoveSuggestionParamsToJSON(const std::string& link_url) {
  std::string result;
  base::ListValue params;
  params.Append(base::Value::CreateStringValue(link_url));
  base::JSONWriter::Write(&params, &result);
  return result;
}

}  // namespace

namespace extensions {

class ExtensionDiscoveryTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    extension_ = utils::CreateEmptyExtensionWithLocation(Manifest::UNPACKED);
  }

  // Runs a function and returns a pointer to a value, transferring ownership.
  base::Value* RunFunctionWithExtension(
      UIThreadExtensionFunction* function, const std::string& args) {
    function->set_extension(extension_.get());
    return utils::RunFunctionAndReturnSingleResult(function, args, browser());
  }

  // Runs a function and ignores the return value.
  void RunFunction(UIThreadExtensionFunction* function,
                   const std::string& args) {
    scoped_ptr<base::Value> result(RunFunctionWithExtension(function, args));
  }

  const std::string& GetExtensionId() const {
    return extension_->id();
  }

 protected:
  scoped_refptr<Extension> extension_;
};

TEST_F(ExtensionDiscoveryTest, Suggest) {
  RunFunction(new DiscoverySuggestFunction(),
      SuggestParamsToJSON("http://www.google.com", "Google", 0.5));

  extensions::SuggestedLinksRegistry* registry =
      extensions::SuggestedLinksRegistryFactory::GetForProfile(
          browser()->profile());

  const SuggestedLinkList* links = registry->GetAll(GetExtensionId());
  ASSERT_EQ(1u, links->size());
  ASSERT_EQ("http://www.google.com", links->at(0)->link_url());
  ASSERT_EQ("Google", links->at(0)->link_text());
  ASSERT_DOUBLE_EQ(0.5, links->at(0)->score());
}

TEST_F(ExtensionDiscoveryTest, SuggestWithoutScore) {
  RunFunction(new DiscoverySuggestFunction(),
      UnscoredSuggestParamsToJSON("https://amazon.com/", "Amazon"));

  extensions::SuggestedLinksRegistry* registry =
      extensions::SuggestedLinksRegistryFactory::GetForProfile(
          browser()->profile());

  const SuggestedLinkList* links = registry->GetAll(GetExtensionId());
  ASSERT_EQ(1u, links->size());
  ASSERT_EQ("https://amazon.com/", links->at(0)->link_url());
  ASSERT_EQ("Amazon", links->at(0)->link_text());
  ASSERT_DOUBLE_EQ(1.0, links->at(0)->score());  // Score should default to 1.
}

TEST_F(ExtensionDiscoveryTest, SuggestTwiceSameUrl) {
  // Suggesting the same URL a second time should override the first.
  RunFunction(new DiscoverySuggestFunction(),
      SuggestParamsToJSON("http://www.google.com", "Google", 0.5));
  RunFunction(new DiscoverySuggestFunction(),
      SuggestParamsToJSON("http://www.google.com", "Google2", 0.1));

  extensions::SuggestedLinksRegistry* registry =
      extensions::SuggestedLinksRegistryFactory::GetForProfile(
          browser()->profile());

  const SuggestedLinkList* links = registry->GetAll(GetExtensionId());
  ASSERT_EQ(1u, links->size());
  ASSERT_EQ("http://www.google.com", links->at(0)->link_url());
  ASSERT_EQ("Google2", links->at(0)->link_text());
  ASSERT_DOUBLE_EQ(0.1, links->at(0)->score());
}

TEST_F(ExtensionDiscoveryTest, Remove) {
  RunFunction(new DiscoverySuggestFunction(),
      UnscoredSuggestParamsToJSON("http://www.google.com", "Google"));
  RunFunction(new DiscoverySuggestFunction(),
      UnscoredSuggestParamsToJSON("https://amazon.com/", "Amazon"));
  RunFunction(new DiscoverySuggestFunction(),
      UnscoredSuggestParamsToJSON("http://www.youtube.com/watch?v=zH5bJSG0DZk",
                                  "YouTube"));
  RunFunction(new DiscoveryRemoveSuggestionFunction(),
      RemoveSuggestionParamsToJSON("https://amazon.com/"));

  extensions::SuggestedLinksRegistry* registry =
      extensions::SuggestedLinksRegistryFactory::GetForProfile(
          browser()->profile());

  const SuggestedLinkList* links = registry->GetAll(GetExtensionId());
  ASSERT_EQ(2u, links->size());
  ASSERT_EQ("http://www.google.com", links->at(0)->link_url());
  ASSERT_EQ("Google", links->at(0)->link_text());
  ASSERT_DOUBLE_EQ(1.0, links->at(0)->score());
  ASSERT_EQ("http://www.youtube.com/watch?v=zH5bJSG0DZk",
      links->at(1)->link_url());
  ASSERT_EQ("YouTube", links->at(1)->link_text());
  ASSERT_DOUBLE_EQ(1.0, links->at(1)->score());
}

TEST_F(ExtensionDiscoveryTest, ClearAll) {
  RunFunction(new DiscoverySuggestFunction(),
      UnscoredSuggestParamsToJSON("http://www.google.com", "Google"));
  RunFunction(new DiscoverySuggestFunction(),
      UnscoredSuggestParamsToJSON("https://amazon.com/", "Amazon"));
  RunFunction(new DiscoverySuggestFunction(),
      UnscoredSuggestParamsToJSON("http://www.youtube.com/watch?v=zH5bJSG0DZk",
                                  "YouTube"));
  RunFunction(new DiscoveryClearAllSuggestionsFunction(), "[]");

  extensions::SuggestedLinksRegistry* registry =
      extensions::SuggestedLinksRegistryFactory::GetForProfile(
          browser()->profile());

  const SuggestedLinkList* links = registry->GetAll(GetExtensionId());
  ASSERT_EQ(0u, links->size());
}

}  // namespace extensions
