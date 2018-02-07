// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/tools/filter_tool.h"

#include <istream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/strings/string_split.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "components/url_pattern_index/url_rule_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

namespace {

url::Origin ParseOrigin(base::StringPiece arg) {
  GURL origin_url(arg);
  LOG_IF(FATAL, !origin_url.is_valid()) << "Invalid origin";
  return url::Origin::Create(origin_url);
}

GURL ParseRequestUrl(base::StringPiece arg) {
  GURL request_url(arg);
  LOG_IF(FATAL, !request_url.is_valid());
  return request_url;
}

url_pattern_index::proto::ElementType ParseType(base::StringPiece type) {
  // If the user provided a resource type, use it. Else if it's the empty string
  // it will default to ELEMENT_TYPE_OTHER.
  if (type == "other")
    return url_pattern_index::proto::ELEMENT_TYPE_OTHER;
  if (type == "script")
    return url_pattern_index::proto::ELEMENT_TYPE_SCRIPT;
  if (type == "image")
    return url_pattern_index::proto::ELEMENT_TYPE_IMAGE;
  if (type == "stylesheet")
    return url_pattern_index::proto::ELEMENT_TYPE_STYLESHEET;
  if (type == "object")
    return url_pattern_index::proto::ELEMENT_TYPE_OBJECT;
  if (type == "xmlhttprequest")
    return url_pattern_index::proto::ELEMENT_TYPE_XMLHTTPREQUEST;
  if (type == "object_subrequest")
    return url_pattern_index::proto::ELEMENT_TYPE_OBJECT_SUBREQUEST;
  if (type == "subdocument")
    return url_pattern_index::proto::ELEMENT_TYPE_SUBDOCUMENT;
  if (type == "ping")
    return url_pattern_index::proto::ELEMENT_TYPE_PING;
  if (type == "media")
    return url_pattern_index::proto::ELEMENT_TYPE_MEDIA;
  if (type == "font")
    return url_pattern_index::proto::ELEMENT_TYPE_FONT;
  if (type == "popup")
    return url_pattern_index::proto::ELEMENT_TYPE_POPUP;
  if (type == "websocket")
    return url_pattern_index::proto::ELEMENT_TYPE_WEBSOCKET;

  return url_pattern_index::proto::ELEMENT_TYPE_OTHER;
}

const url_pattern_index::flat::UrlRule* FindMatchingUrlRule(
    const subresource_filter::MemoryMappedRuleset* ruleset,
    const url::Origin& document_origin,
    const GURL& request_url,
    url_pattern_index::proto::ElementType type) {
  subresource_filter::DocumentSubresourceFilter filter(
      document_origin,
      subresource_filter::ActivationState(
          subresource_filter::ActivationLevel::ENABLED),
      ruleset);

  return filter.FindMatchingUrlRule(request_url, type);
}

}  // namespace

FilterTool::FilterTool(
    scoped_refptr<const subresource_filter::MemoryMappedRuleset> ruleset,
    std::ostream* output)
    : ruleset_(std::move(ruleset)), output_(output) {}

FilterTool::~FilterTool() = default;

void FilterTool::Match(const std::string& document_origin,
                       const std::string& url,
                       const std::string& type) {
  bool blocked;
  const url_pattern_index::flat::UrlRule* rule =
      MatchImpl(document_origin, url, type, &blocked);
  PrintResult(blocked, rule, document_origin, url, type);
}

void FilterTool::MatchBatch(std::istream* request_stream) {
  MatchBatchImpl(request_stream, true /* print each request */, 1);
}

void FilterTool::MatchRules(std::istream* request_stream, int min_match_count) {
  MatchBatchImpl(request_stream, false /* print each request */,
                 min_match_count);
}

void FilterTool::PrintResult(bool blocked,
                             const url_pattern_index::flat::UrlRule* rule,
                             base::StringPiece document_origin,
                             base::StringPiece url,
                             base::StringPiece type) {
  *output_ << (blocked ? "BLOCKED " : "ALLOWED ");
  if (rule) {
    *output_ << url_pattern_index::FlatUrlRuleToString(rule) << " ";
  }
  *output_ << document_origin << " " << url << " " << type << std::endl;
}

const url_pattern_index::flat::UrlRule* FilterTool::MatchImpl(
    base::StringPiece document_origin,
    base::StringPiece url,
    base::StringPiece type,
    bool* blocked) {
  const url_pattern_index::flat::UrlRule* rule =
      FindMatchingUrlRule(ruleset_.get(), ParseOrigin(document_origin),
                          ParseRequestUrl(url), ParseType(type));

  *blocked = rule && !(rule->options() &
                       url_pattern_index::flat::OptionFlag_IS_WHITELIST);
  return rule;
}

// If |print_each_request| is true, then the result of each match is written
// to |output_|, just as in Match. Otherwise, the set of matching rules is
// written to |output_|.
void FilterTool::MatchBatchImpl(std::istream* request_stream,
                                bool print_each_request,
                                int min_match_count) {
  std::unordered_map<const url_pattern_index::flat::UrlRule*, int>
      matched_rules;

  std::string line;
  while (std::getline(*request_stream, line)) {
    if (line.empty())
      continue;

    std::vector<base::StringPiece> strings = base::SplitStringPiece(
        line, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    LOG_IF(FATAL, strings.size() != 3u)
        << "Incorrect number of arguments found in batch record";

    base::StringPiece origin = strings[0];
    base::StringPiece request_url = strings[1];
    base::StringPiece request_type = strings[2];

    bool blocked;
    const url_pattern_index::flat::UrlRule* rule =
        MatchImpl(origin, request_url, request_type, &blocked);
    if (rule)
      matched_rules[rule] += 1;

    if (print_each_request)
      PrintResult(blocked, rule, origin, request_url, request_type);
  }

  if (print_each_request)
    return;

  for (auto rule_and_count : matched_rules) {
    if (rule_and_count.second >= min_match_count) {
      *output_ << url_pattern_index::FlatUrlRuleToString(rule_and_count.first)
                      .c_str()
               << std::endl;
    }
  }
}

}  // namespace subresource_filter
