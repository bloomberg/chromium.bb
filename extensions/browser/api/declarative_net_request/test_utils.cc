// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/test_utils.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {

namespace dnr_api = api::declarative_net_request;

RequestAction CreateRequestActionForTesting(RequestAction::Type type,
                                            uint32_t rule_id,
                                            uint32_t rule_priority,
                                            dnr_api::SourceType source_type,
                                            const ExtensionId& extension_id) {
  return RequestAction(type, rule_id, rule_priority, source_type, extension_id);
}

// Note: This is not declared in the anonymous namespace so that we can use it
// with gtest. This reuses the logic used to test action equality in
// TestRequestACtion in test_utils.h.
bool operator==(const RequestAction& lhs, const RequestAction& rhs) {
  static_assert(flat::ActionIndex_count == 7,
                "Modify this method to ensure it stays updated as new actions "
                "are added.");

  auto are_vectors_equal = [](std::vector<const char*> a,
                              std::vector<const char*> b) {
    return std::set<base::StringPiece>(a.begin(), a.end()) ==
           std::set<base::StringPiece>(b.begin(), b.end());
  };

  auto get_members_tuple = [](const RequestAction& action) {
    return std::tie(action.type, action.redirect_url, action.rule_id,
                    action.rule_priority, action.source_type,
                    action.extension_id);
  };

  return get_members_tuple(lhs) == get_members_tuple(rhs) &&
         are_vectors_equal(lhs.request_headers_to_remove,
                           rhs.request_headers_to_remove) &&
         are_vectors_equal(lhs.response_headers_to_remove,
                           rhs.response_headers_to_remove);
}

std::ostream& operator<<(std::ostream& output, RequestAction::Type type) {
  switch (type) {
    case RequestAction::Type::BLOCK:
      output << "BLOCK";
      break;
    case RequestAction::Type::COLLAPSE:
      output << "COLLAPSE";
      break;
    case RequestAction::Type::ALLOW:
      output << "ALLOW";
      break;
    case RequestAction::Type::REDIRECT:
      output << "REDIRECT";
      break;
    case RequestAction::Type::REMOVE_HEADERS:
      output << "REMOVE_HEADERS";
      break;
  }
  return output;
}

std::ostream& operator<<(std::ostream& output, const RequestAction& action) {
  output << "\nRequestAction\n";
  output << "|type| " << action.type << "\n";
  output << "|redirect_url| "
         << (action.redirect_url ? action.redirect_url->spec()
                                 : std::string("nullopt"))
         << "\n";
  output << "|rule_id| " << action.rule_id << "\n";
  output << "|rule_priority| " << action.rule_priority << "\n";
  output << "|source_type| "
         << api::declarative_net_request::ToString(action.source_type) << "\n";
  output << "|extension_id| " << action.extension_id << "\n";
  output << "|request_headers_to_remove| "
         << ::testing::PrintToString(action.request_headers_to_remove) << "\n";
  output << "|response_headers_to_remove| "
         << ::testing::PrintToString(action.response_headers_to_remove);
  return output;
}

std::ostream& operator<<(std::ostream& output, const ParseResult& result) {
  switch (result) {
    case ParseResult::SUCCESS:
      output << "SUCCESS";
      break;
    case ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED:
      output << "ERROR_RESOURCE_TYPE_DUPLICATED";
      break;
    case ParseResult::ERROR_EMPTY_REDIRECT_RULE_PRIORITY:
      output << "ERROR_EMPTY_REDIRECT_RULE_PRIORITY";
      break;
    case ParseResult::ERROR_EMPTY_UPGRADE_RULE_PRIORITY:
      output << "ERROR_EMPTY_UPGRADE_RULE_PRIORITY";
      break;
    case ParseResult::ERROR_INVALID_RULE_ID:
      output << "ERROR_INVALID_RULE_ID";
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_RULE_PRIORITY:
      output << "ERROR_INVALID_REDIRECT_RULE_PRIORITY";
      break;
    case ParseResult::ERROR_INVALID_UPGRADE_RULE_PRIORITY:
      output << "ERROR_INVALID_UPGRADE_RULE_PRIORITY";
      break;
    case ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES:
      output << "ERROR_NO_APPLICABLE_RESOURCE_TYPES";
      break;
    case ParseResult::ERROR_EMPTY_DOMAINS_LIST:
      output << "ERROR_EMPTY_DOMAINS_LIST";
      break;
    case ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST:
      output << "ERROR_EMPTY_RESOURCE_TYPES_LIST";
      break;
    case ParseResult::ERROR_EMPTY_URL_FILTER:
      output << "ERROR_EMPTY_URL_FILTER";
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_URL:
      output << "ERROR_INVALID_REDIRECT_URL";
      break;
    case ParseResult::ERROR_DUPLICATE_IDS:
      output << "ERROR_DUPLICATE_IDS";
      break;
    case ParseResult::ERROR_PERSISTING_RULESET:
      output << "ERROR_PERSISTING_RULESET";
      break;
    case ParseResult::ERROR_NON_ASCII_URL_FILTER:
      output << "ERROR_NON_ASCII_URL_FILTER";
      break;
    case ParseResult::ERROR_NON_ASCII_DOMAIN:
      output << "ERROR_NON_ASCII_DOMAIN";
      break;
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN:
      output << "ERROR_NON_ASCII_EXCLUDED_DOMAIN";
      break;
    case ParseResult::ERROR_INVALID_URL_FILTER:
      output << "ERROR_INVALID_URL_FILTER";
      break;
    case ParseResult::ERROR_EMPTY_REMOVE_HEADERS_LIST:
      output << "ERROR_EMPTY_REMOVE_HEADERS_LIST";
      break;
    case ParseResult::ERROR_INVALID_REDIRECT:
      output << "ERROR_INVALID_REDIRECT";
      break;
    case ParseResult::ERROR_INVALID_EXTENSION_PATH:
      output << "ERROR_INVALID_EXTENSION_PATH";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_SCHEME:
      output << "ERROR_INVALID_TRANSFORM_SCHEME";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_PORT:
      output << "ERROR_INVALID_TRANSFORM_PORT";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_QUERY:
      output << "ERROR_INVALID_TRANSFORM_QUERY";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT:
      output << "ERROR_INVALID_TRANSFORM_FRAGMENT";
      break;
    case ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED:
      output << "ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED";
      break;
    case ParseResult::ERROR_JAVASCRIPT_REDIRECT:
      output << "ERROR_JAVASCRIPT_REDIRECT";
      break;
    case ParseResult::ERROR_EMPTY_REGEX_FILTER:
      output << "ERROR_EMPTY_REGEX_FILTER";
      break;
    case ParseResult::ERROR_NON_ASCII_REGEX_FILTER:
      output << "ERROR_NON_ASCII_REGEX_FILTER";
      break;
    case ParseResult::ERROR_INVALID_REGEX_FILTER:
      output << "ERROR_INVALID_REGEX_FILTER";
      break;
    case ParseResult::ERROR_MULTIPLE_FILTERS_SPECIFIED:
      output << "ERROR_MULTIPLE_FILTERS_SPECIFIED";
      break;
    case ParseResult::ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER:
      output << "ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER";
      break;
    case ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION:
      output << "ERROR_INVALID_REGEX_SUBSTITUTION";
      break;
  }
  return output;
}

bool HasValidIndexedRuleset(const Extension& extension,
                            content::BrowserContext* browser_context) {
  int expected_checksum;
  if (!ExtensionPrefs::Get(browser_context)
           ->GetDNRRulesetChecksum(extension.id(), &expected_checksum)) {
    return false;
  }

  std::unique_ptr<RulesetMatcher> matcher;
  return RulesetMatcher::CreateVerifiedMatcher(
             RulesetSource::CreateStatic(extension), expected_checksum,
             &matcher) == RulesetMatcher::kLoadSuccess;
}

bool CreateVerifiedMatcher(const std::vector<TestRule>& rules,
                           const RulesetSource& source,
                           std::unique_ptr<RulesetMatcher>* matcher,
                           int* expected_checksum) {
  // Serialize |rules|.
  ListBuilder builder;
  for (const auto& rule : rules)
    builder.Append(rule.ToValue());
  JSONFileValueSerializer(source.json_path()).Serialize(*builder.Build());

  // Index ruleset.
  IndexAndPersistJSONRulesetResult result =
      source.IndexAndPersistJSONRulesetUnsafe();
  if (!result.success) {
    DCHECK(result.error.empty()) << result.error;
    return false;
  }

  if (!result.warnings.empty())
    return false;

  if (expected_checksum)
    *expected_checksum = result.ruleset_checksum;

  // Create verified matcher.
  RulesetMatcher::LoadRulesetResult load_result =
      RulesetMatcher::CreateVerifiedMatcher(source, result.ruleset_checksum,
                                            matcher);
  return load_result == RulesetMatcher::kLoadSuccess;
}

RulesetSource CreateTemporarySource(size_t id,
                                    size_t priority,
                                    dnr_api::SourceType source_type,
                                    size_t rule_count_limit,
                                    ExtensionId extension_id) {
  std::unique_ptr<RulesetSource> source = RulesetSource::CreateTemporarySource(
      id, priority, source_type, rule_count_limit, std::move(extension_id));
  CHECK(source);
  return source->Clone();
}

}  // namespace declarative_net_request
}  // namespace extensions
