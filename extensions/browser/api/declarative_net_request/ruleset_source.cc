// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_source.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat_ruleset_indexer.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/file_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/service_manager/public/cpp/identity.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = extensions::api::declarative_net_request;

// TODO(crbug.com/930961): Change once multiple rulesets within an extension are
// supported.
const size_t kDefaultRulesetID = 0;
const size_t kDefaultRulesetPriority = 0;

// Helper to retrieve the filename for the given |file_path|.
std::string GetFilename(const base::FilePath& file_path) {
  return file_path.BaseName().AsUTF8Unsafe();
}

std::string GetJSONParseError(const std::string& json_ruleset_filename,
                              const std::string& json_parse_error) {
  return base::StrCat({json_ruleset_filename, ": ", json_parse_error});
}

InstallWarning CreateInstallWarning(const std::string& message) {
  return InstallWarning(message, manifest_keys::kDeclarativeNetRequestKey,
                        manifest_keys::kDeclarativeRuleResourcesKey);
}

// Helper function to index |rules| and persist them in indexed/flatbuffer
// format.
ParseInfo IndexAndPersistRulesImpl(const base::Value& rules,
                                   const RulesetSource& source,
                                   std::vector<InstallWarning>* warnings,
                                   int* ruleset_checksum,
                                   size_t* rules_count) {
  DCHECK(warnings);
  DCHECK(ruleset_checksum);
  DCHECK(rules_count);

  if (!rules.is_list())
    return ParseInfo(ParseResult::ERROR_LIST_NOT_PASSED);

  FlatRulesetIndexer indexer;

  bool rule_count_exceeded = false;

  // Limit the maximum number of rule unparsed warnings to 5.
  const size_t kMaxUnparsedRulesWarnings = 5;
  bool unparsed_warnings_limit_exeeded = false;
  size_t unparsed_warning_count = 0;

  {
    std::set<int> id_set;  // Ensure all ids are distinct.

    const auto& rules_list = rules.GetList();
    for (size_t i = 0; i < rules_list.size(); i++) {
      dnr_api::Rule parsed_rule;
      base::string16 parse_error;

      // Ignore rules which can't be successfully parsed and show an install
      // warning for them. A hard error is not thrown to maintain backwards
      // compatibility.
      if (!dnr_api::Rule::Populate(rules_list[i], &parsed_rule, &parse_error) ||
          !parse_error.empty()) {
        if (unparsed_warning_count < kMaxUnparsedRulesWarnings) {
          ++unparsed_warning_count;
          std::string rule_location;

          // If possible use the rule ID in the install warning.
          if (auto* id_val = rules_list[i].FindKeyOfType(
                  kIDKey, base::Value::Type::INTEGER)) {
            rule_location = base::StringPrintf("id %d", id_val->GetInt());
          } else {
            // Use one-based indices.
            rule_location = base::StringPrintf("index %zu", i + 1);
          }

          warnings->push_back(
              CreateInstallWarning(ErrorUtils::FormatErrorMessage(
                  kRuleNotParsedWarning, rule_location,
                  base::UTF16ToUTF8(parse_error))));
        } else {
          unparsed_warnings_limit_exeeded = true;
        }
        continue;
      }

      int rule_id = parsed_rule.id;
      bool inserted = id_set.insert(rule_id).second;
      if (!inserted)
        return ParseInfo(ParseResult::ERROR_DUPLICATE_IDS, rule_id);

      IndexedRule indexed_rule;
      ParseResult parse_result =
          IndexedRule::CreateIndexedRule(std::move(parsed_rule), &indexed_rule);
      if (parse_result != ParseResult::SUCCESS)
        return ParseInfo(parse_result, rule_id);

      if (indexer.indexed_rules_count() >= source.rule_count_limit()) {
        rule_count_exceeded = true;
        break;
      }

      indexer.AddUrlRule(indexed_rule);
    }
  }
  indexer.Finish();

  *rules_count = indexer.indexed_rules_count();

  if (!PersistIndexedRuleset(source.indexed_path(), indexer.GetData(),
                             ruleset_checksum))
    return ParseInfo(ParseResult::ERROR_PERSISTING_RULESET);

  if (rule_count_exceeded)
    warnings->push_back(CreateInstallWarning(kRuleCountExceeded));

  if (unparsed_warnings_limit_exeeded) {
    DCHECK_EQ(kMaxUnparsedRulesWarnings, unparsed_warning_count);
    warnings->push_back(CreateInstallWarning(ErrorUtils::FormatErrorMessage(
        kTooManyParseFailuresWarning,
        std::to_string(kMaxUnparsedRulesWarnings))));
  }

  return ParseInfo(ParseResult::SUCCESS);
}

void OnSafeJSONParserSuccess(
    const RulesetSource& source,
    RulesetSource::IndexAndPersistRulesCallback callback,
    std::unique_ptr<base::Value> root) {
  DCHECK(root);

  std::vector<InstallWarning> warnings;
  int ruleset_checksum;
  size_t rules_count;
  base::ElapsedTimer timer;
  const ParseInfo info = IndexAndPersistRulesImpl(
      *root, source, &warnings, &ruleset_checksum, &rules_count);
  if (info.result() == ParseResult::SUCCESS) {
    std::move(callback).Run(IndexAndPersistRulesResult::CreateSuccessResult(
        ruleset_checksum, std::move(warnings), rules_count, timer.Elapsed()));
    return;
  }

  std::string error = info.GetErrorDescription(GetFilename(source.json_path()));
  std::move(callback).Run(
      IndexAndPersistRulesResult::CreateErrorResult(std::move(error)));
}

void OnSafeJSONParserError(RulesetSource::IndexAndPersistRulesCallback callback,
                           const std::string& json_ruleset_filename,
                           const std::string& json_parse_error) {
  std::move(callback).Run(IndexAndPersistRulesResult::CreateErrorResult(
      GetJSONParseError(json_ruleset_filename, json_parse_error)));
}

}  // namespace

// static
IndexAndPersistRulesResult IndexAndPersistRulesResult::CreateSuccessResult(
    int ruleset_checksum,
    std::vector<InstallWarning> warnings,
    size_t rules_count,
    base::TimeDelta index_and_persist_time) {
  IndexAndPersistRulesResult result;
  result.success = true;
  result.ruleset_checksum = ruleset_checksum;
  result.warnings = std::move(warnings);
  result.rules_count = rules_count;
  result.index_and_persist_time = index_and_persist_time;
  return result;
}

// static
IndexAndPersistRulesResult IndexAndPersistRulesResult::CreateErrorResult(
    std::string error) {
  IndexAndPersistRulesResult result;
  result.success = false;
  result.error = std::move(error);
  return result;
}

IndexAndPersistRulesResult::~IndexAndPersistRulesResult() = default;
IndexAndPersistRulesResult::IndexAndPersistRulesResult(
    IndexAndPersistRulesResult&&) = default;
IndexAndPersistRulesResult& IndexAndPersistRulesResult::operator=(
    IndexAndPersistRulesResult&&) = default;
IndexAndPersistRulesResult::IndexAndPersistRulesResult() = default;

// static
RulesetSource RulesetSource::Create(const Extension& extension) {
  return RulesetSource(
      declarative_net_request::DNRManifestData::GetRulesetPath(extension),
      file_util::GetIndexedRulesetPath(extension.path()), kDefaultRulesetID,
      kDefaultRulesetPriority, dnr_api::MAX_NUMBER_OF_RULES);
}

RulesetSource::RulesetSource(base::FilePath json_path,
                             base::FilePath indexed_path,
                             size_t id,
                             size_t priority,
                             size_t rule_count_limit)
    : json_path_(std::move(json_path)),
      indexed_path_(std::move(indexed_path)),
      id_(id),
      priority_(priority),
      rule_count_limit_(rule_count_limit) {}

RulesetSource::~RulesetSource() = default;
RulesetSource::RulesetSource(RulesetSource&&) = default;
RulesetSource& RulesetSource::operator=(RulesetSource&&) = default;

RulesetSource RulesetSource::Clone() const {
  return RulesetSource(json_path_, indexed_path_, id_, priority_,
                       rule_count_limit_);
}

IndexAndPersistRulesResult RulesetSource::IndexAndPersistRulesUnsafe() const {
  DCHECK(IsAPIAvailable());

  JSONFileValueDeserializer deserializer(json_path_);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(
      nullptr /*error_code*/, &error /*error_message*/);
  if (!root) {
    return IndexAndPersistRulesResult::CreateErrorResult(
        GetJSONParseError(GetFilename(json_path_), error));
  }

  std::vector<InstallWarning> warnings;
  int ruleset_checksum;
  size_t rules_count;
  base::ElapsedTimer timer;
  const ParseInfo info = IndexAndPersistRulesImpl(
      *root, *this, &warnings, &ruleset_checksum, &rules_count);
  if (info.result() == ParseResult::SUCCESS) {
    return IndexAndPersistRulesResult::CreateSuccessResult(
        ruleset_checksum, std::move(warnings), rules_count, timer.Elapsed());
  }

  error = info.GetErrorDescription(GetFilename(json_path_));
  return IndexAndPersistRulesResult::CreateErrorResult(std::move(error));
}

void RulesetSource::IndexAndPersistRules(
    service_manager::Connector* connector,
    const base::Optional<base::Token>& decoder_batch_id,
    IndexAndPersistRulesCallback callback) const {
  DCHECK(IsAPIAvailable());

  std::string json_contents;
  if (!base::ReadFileToString(json_path_, &json_contents)) {
    std::move(callback).Run(IndexAndPersistRulesResult::CreateErrorResult(
        manifest_errors::kDeclarativeNetRequestJSONRulesFileReadError));
    return;
  }

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  auto error_callback = base::BindRepeating(
      &OnSafeJSONParserError, repeating_callback, GetFilename(json_path_));
  auto success_callback = base::BindRepeating(&OnSafeJSONParserSuccess, Clone(),
                                              repeating_callback);

  if (decoder_batch_id) {
    data_decoder::SafeJsonParser::ParseBatch(connector, json_contents,
                                             success_callback, error_callback,
                                             *decoder_batch_id);
  } else {
    data_decoder::SafeJsonParser::Parse(connector, json_contents,
                                        success_callback, error_callback);
  }
}

}  // namespace declarative_net_request
}  // namespace extensions
