// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/file_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {
namespace {

enum class ExtensionLoadType {
  UNPACKED,
};

constexpr char kJSONRulesFilename[] = "rules_file.json";
const base::FilePath::CharType kJSONRulesetFilepath[] =
    FILE_PATH_LITERAL("rules_file.json");

// Fixure testing that declarative rules corresponding to the Declarative Net
// Request API are correctly indexed. Currently only unpacked extensions are
// tested.
class RuleIndexingTest
    : public ExtensionServiceTestBase,
      public ::testing::WithParamInterface<ExtensionLoadType> {
 public:
  // Use channel UNKNOWN to ensure that the declarativeNetRequest API is
  // available, irrespective of its actual availability.
  RuleIndexingTest() : channel_(::version_info::Channel::UNKNOWN) {}

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    loader_ = std::make_unique<ChromeTestExtensionLoader>(browser_context());
    EXPECT_EQ(ExtensionLoadType::UNPACKED, GetParam());
    loader_->set_pack_extension(false);
  }

 protected:
  void AddRule(const TestRule& rule) { rules_list_.push_back(rule); }

  // This takes precedence over the AddRule method.
  void SetRules(std::unique_ptr<base::Value> rules) {
    rules_value_ = std::move(rules);
  }

  // Loads the extension and verifies the indexed ruleset location and histogram
  // counts.
  void LoadAndExpectSuccess(size_t expected_indexed_rules_count) {
    base::HistogramTester tester;
    WriteManifestAndRuleset();

    loader_->set_should_fail(false);

    // Clear all load errors before loading the extension.
    error_reporter()->ClearErrors();

    extension_ = loader_->LoadExtension(extension_dir_);
    ASSERT_TRUE(extension_.get());
    EXPECT_TRUE(
        base::PathExists(file_util::GetIndexedRulesetPath(extension_->path())));

    // Ensure no load errors were reported.
    EXPECT_TRUE(error_reporter()->GetErrors()->empty());

    tester.ExpectTotalCount(kIndexRulesTimeHistogram, 1);
    tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram, 1);
    tester.ExpectBucketCount(kManifestRulesCountHistogram,
                             expected_indexed_rules_count, 1);
  }

  void LoadAndExpectError(const std::string& expected_error_suffix) {
    base::HistogramTester tester;
    WriteManifestAndRuleset();

    loader_->set_should_fail(true);

    // Clear all load errors before loading the extension.
    error_reporter()->ClearErrors();

    extension_ = loader_->LoadExtension(extension_dir_);
    EXPECT_FALSE(extension_.get());

    // Verify the error. ExtensionErrorReporter may prepend some text to the
    // actual load error.
    const std::vector<base::string16>* errors = error_reporter()->GetErrors();
    ASSERT_EQ(1u, errors->size());
    EXPECT_TRUE(base::EndsWith(errors->at(0),
                               base::UTF8ToUTF16(expected_error_suffix),
                               base::CompareCase::SENSITIVE))
        << "expected: " << expected_error_suffix
        << " actual: " << errors->at(0);

    tester.ExpectTotalCount(kIndexRulesTimeHistogram, 0u);
    tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram, 0u);
    tester.ExpectTotalCount(kManifestRulesCountHistogram, 0u);
  }

  void set_persist_invalid_json_file() { persist_invalid_json_file_ = true; }

  ChromeTestExtensionLoader* extension_loader() { return loader_.get(); }

  const Extension* extension() const { return extension_.get(); }
  void set_extension(scoped_refptr<const Extension> extension) {
    extension_ = extension;
  }

 private:
  void WriteManifestAndRuleset() {
    if (!rules_value_) {
      ListBuilder builder;
      for (const auto& rule : rules_list_)
        builder.Append(rule.ToValue());
      rules_value_ = builder.Build();
    }

    extension_dir_ =
        temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension"));

    // Create extension directory.
    EXPECT_TRUE(base::CreateDirectory(extension_dir_));

    // Persist JSON rules file.
    base::FilePath json_rules_path =
        extension_dir_.Append(kJSONRulesetFilepath);
    if (!persist_invalid_json_file_) {
      JSONFileValueSerializer(json_rules_path).Serialize(*rules_value_);
    } else {
      std::string data = "invalid json";
      base::WriteFile(json_rules_path, data.c_str(), data.size());
    }

    // Persist manifest file.
    JSONFileValueSerializer(extension_dir_.Append(kManifestFilename))
        .Serialize(*CreateManifest(kJSONRulesFilename));
  }

  ExtensionErrorReporter* error_reporter() {
    return ExtensionErrorReporter::GetInstance();
  }

  ScopedCurrentChannel channel_;
  std::vector<TestRule> rules_list_;
  std::unique_ptr<base::Value> rules_value_;
  base::FilePath extension_dir_;
  std::unique_ptr<ChromeTestExtensionLoader> loader_;
  scoped_refptr<const Extension> extension_;
  bool persist_invalid_json_file_ = false;

  DISALLOW_COPY_AND_ASSIGN(RuleIndexingTest);
};

TEST_P(RuleIndexingTest, DuplicateResourceTypes) {
  TestRule rule = CreateGenericRule();
  rule.condition->resource_types =
      std::vector<std::string>({"image", "stylesheet"});
  rule.condition->excluded_resource_types = std::vector<std::string>({"image"});
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyRedirectRulePriority) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("https://google.com");
  AddRule(rule);
  LoadAndExpectError(
      ParseInfo(ParseResult::ERROR_EMPTY_REDIRECT_RULE_PRIORITY, 0u)
          .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyRedirectRuleUrl) {
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  AddRule(rule);

  rule.id = kMinValidID + 1;
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  AddRule(rule);

  LoadAndExpectError(ParseInfo(ParseResult::ERROR_EMPTY_REDIRECT_URL, 1u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, InvalidRuleID) {
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID - 1;
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_INVALID_RULE_ID, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, InvalidRedirectRulePriority) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("https://google.com");
  rule.priority = kMinValidPriority - 1;
  AddRule(rule);
  LoadAndExpectError(
      ParseInfo(ParseResult::ERROR_INVALID_REDIRECT_RULE_PRIORITY, 0u)
          .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, NoApplicableResourceTypes) {
  TestRule rule = CreateGenericRule();
  rule.condition->excluded_resource_types = std::vector<std::string>(
      {"sub_frame", "stylesheet", "script", "image", "font", "object",
       "xmlhttprequest", "ping", "media", "websocket", "other"});
  AddRule(rule);
  LoadAndExpectError(
      ParseInfo(ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES, 0u)
          .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->domains = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_EMPTY_DOMAINS_LIST, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyResourceTypeList) {
  TestRule rule = CreateGenericRule();
  rule.condition->resource_types = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyURLFilter) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string();
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_EMPTY_URL_FILTER, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, InvalidRedirectURL) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("google");
  rule.priority = kMinValidPriority;
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_INVALID_REDIRECT_URL, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, ListNotPassed) {
  SetRules(std::make_unique<base::DictionaryValue>());
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_LIST_NOT_PASSED)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, DuplicateIDS) {
  TestRule rule = CreateGenericRule();
  AddRule(rule);
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_DUPLICATE_IDS, 1u)
                         .GetErrorDescription(kJSONRulesFilename));
}

// Ensures that rules which can't be parsed are ignored and cause an install
// warning.
TEST_P(RuleIndexingTest, InvalidJSONRule) {
  TestRule rule = CreateGenericRule();
  AddRule(rule);

  rule.id = kMinValidID + 1;
  rule.action->type = std::string("invalid action");
  AddRule(rule);

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(1 /* rules count */);

  ASSERT_EQ(1u, extension()->install_warnings().size());
  EXPECT_EQ(InstallWarning(kRulesNotParsedWarning,
                           manifest_keys::kDeclarativeNetRequestKey,
                           manifest_keys::kDeclarativeRuleResourcesKey),
            extension()->install_warnings()[0]);
}

TEST_P(RuleIndexingTest, InvalidJSONFile) {
  set_persist_invalid_json_file();
  // The error is returned by the JSON parser we use. Hence just test that an
  // error is thrown without verifying what it is.
  LoadAndExpectError("");
}

TEST_P(RuleIndexingTest, EmptyRuleset) {
  LoadAndExpectSuccess(0 /* rules count */);
}

TEST_P(RuleIndexingTest, AddSingleRule) {
  AddRule(CreateGenericRule());
  LoadAndExpectSuccess(1 /* rules count */);
}

TEST_P(RuleIndexingTest, AddTwoRules) {
  TestRule rule = CreateGenericRule();
  AddRule(rule);

  rule.id = kMinValidID + 1;
  AddRule(rule);
  LoadAndExpectSuccess(2 /* rules count */);
}

TEST_P(RuleIndexingTest, ReloadExtension) {
  AddRule(CreateGenericRule());
  LoadAndExpectSuccess(1 /* rules count */);

  base::HistogramTester tester;
  TestExtensionRegistryObserver registry_observer(registry());

  service()->ReloadExtension(extension()->id());
  // Reloading should invalidate pointers to existing extension(). Hence reset
  // it.
  set_extension(make_scoped_refptr(registry_observer.WaitForExtensionLoaded()));

  // Reloading the extension should cause the rules to be re-indexed in the
  // case of unpacked extensions.
  int expected_histogram_count = 1;
  EXPECT_EQ(ExtensionLoadType::UNPACKED, GetParam());
  tester.ExpectTotalCount(kIndexRulesTimeHistogram, expected_histogram_count);
  tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram,
                          expected_histogram_count);
  tester.ExpectBucketCount(kManifestRulesCountHistogram, 1 /* rules count */,
                           expected_histogram_count);

  // Ensure no install warnings were raised on reload.
  EXPECT_TRUE(extension()->install_warnings().empty());
}

INSTANTIATE_TEST_CASE_P(,
                        RuleIndexingTest,
                        ::testing::Values(ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
