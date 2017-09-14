// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_

namespace extensions {
namespace declarative_net_request {

// The result of parsing JSON rules provided by an extension. Can correspond to
// a single or multiple rules.
enum class ParseResult {
  SUCCESS,
  ERROR_RESOURCE_TYPE_DUPLICATED,
  ERROR_EMPTY_REDIRECT_RULE_PRIORITY,
  ERROR_EMPTY_REDIRECT_URL,
  ERROR_INVALID_RULE_ID,
  ERROR_INVALID_REDIRECT_RULE_PRIORITY,
  ERROR_NO_APPLICABLE_RESOURCE_TYPES,
  ERROR_EMPTY_DOMAINS_LIST,
  ERROR_EMPTY_RESOURCE_TYPES_LIST,
  ERROR_EMPTY_URL_FILTER,
  ERROR_INVALID_REDIRECT_URL,
  ERROR_LIST_NOT_PASSED,
  ERROR_DUPLICATE_IDS,
  ERROR_PERSISTING_RULESET,
};

// Rule parsing errors.
extern const char kErrorResourceTypeDuplicated[];
extern const char kErrorEmptyRedirectRuleKey[];
extern const char kErrorInvalidRuleKey[];
extern const char kErrorNoApplicableResourceTypes[];
extern const char kErrorEmptyList[];
extern const char kErrorEmptyUrlFilter[];
extern const char kErrorInvalidRedirectUrl[];
extern const char kErrorListNotPassed[];
extern const char kErrorDuplicateIDs[];
extern const char kErrorPersisting[];

// Rule parsing install warnings.
extern const char kRulesNotParsedWarning[];

// Minimum valid value of a declarative rule ID.
constexpr int kMinValidID = 1;

// Minimum valid value of a declarative rule priority.
constexpr int kMinValidPriority = 1;

// Default priority used for rules when the priority is not explicity provided
// by an extension.
constexpr int kDefaultPriority = 1;

// Keys used in rules.
extern const char kIDKey[];
extern const char kPriorityKey[];
extern const char kUrlFilterKey[];
extern const char kIsUrlFilterCaseSensitiveKey[];
extern const char kDomainsKey[];
extern const char kResourceTypesKey[];
extern const char kRedirectUrlKey[];

// Histogram names.
extern const char kIndexAndPersistRulesTimeHistogram[];
extern const char kIndexRulesTimeHistogram[];
extern const char kManifestRulesCountHistogram[];

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_
