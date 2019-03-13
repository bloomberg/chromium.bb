// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <utility>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/common/resource_type.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;

namespace {

using FindRuleStrategy =
    url_pattern_index::UrlPatternIndexMatcher::FindRuleStrategy;

// Don't exclude generic rules from being matched. A generic rule is one with
// an empty included domains list.
const bool kDisableGenericRules = false;

// Maps content::ResourceType to flat_rule::ElementType.
flat_rule::ElementType GetElementType(content::ResourceType type) {
  switch (type) {
    case content::RESOURCE_TYPE_LAST_TYPE:
    case content::RESOURCE_TYPE_PREFETCH:
    case content::RESOURCE_TYPE_SUB_RESOURCE:
    case content::RESOURCE_TYPE_NAVIGATION_PRELOAD:
      return flat_rule::ElementType_OTHER;
    case content::RESOURCE_TYPE_MAIN_FRAME:
      return flat_rule::ElementType_MAIN_FRAME;
    case content::RESOURCE_TYPE_CSP_REPORT:
      return flat_rule::ElementType_CSP_REPORT;
    case content::RESOURCE_TYPE_SCRIPT:
    case content::RESOURCE_TYPE_WORKER:
    case content::RESOURCE_TYPE_SHARED_WORKER:
    case content::RESOURCE_TYPE_SERVICE_WORKER:
      return flat_rule::ElementType_SCRIPT;
    case content::RESOURCE_TYPE_IMAGE:
    case content::RESOURCE_TYPE_FAVICON:
      return flat_rule::ElementType_IMAGE;
    case content::RESOURCE_TYPE_STYLESHEET:
      return flat_rule::ElementType_STYLESHEET;
    case content::RESOURCE_TYPE_OBJECT:
    case content::RESOURCE_TYPE_PLUGIN_RESOURCE:
      return flat_rule::ElementType_OBJECT;
    case content::RESOURCE_TYPE_XHR:
      return flat_rule::ElementType_XMLHTTPREQUEST;
    case content::RESOURCE_TYPE_SUB_FRAME:
      return flat_rule::ElementType_SUBDOCUMENT;
    case content::RESOURCE_TYPE_PING:
      return flat_rule::ElementType_PING;
    case content::RESOURCE_TYPE_MEDIA:
      return flat_rule::ElementType_MEDIA;
    case content::RESOURCE_TYPE_FONT_RESOURCE:
      return flat_rule::ElementType_FONT;
  }
  NOTREACHED();
  return flat_rule::ElementType_OTHER;
}

// Returns the flat_rule::ElementType for the given |request|.
flat_rule::ElementType GetElementType(const WebRequestInfo& request) {
  if (request.url.SchemeIsWSOrWSS())
    return flat_rule::ElementType_WEBSOCKET;

  return request.type.has_value() ? GetElementType(request.type.value())
                                  : flat_rule::ElementType_OTHER;
}

// Returns whether the request to |url| is third party to its |document_origin|.
// TODO(crbug.com/696822): Look into caching this.
bool IsThirdPartyRequest(const GURL& url, const url::Origin& document_origin) {
  if (document_origin.opaque())
    return true;

  return !net::registry_controlled_domains::SameDomainOrHost(
      url, document_origin,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

RequestParams::RequestParams(const WebRequestInfo& info)
    : url(&info.url),
      first_party_origin(info.initiator.value_or(url::Origin())),
      element_type(GetElementType(info)) {
  is_third_party = IsThirdPartyRequest(*url, first_party_origin);
}

RequestParams::RequestParams() = default;

// static
RulesetMatcher::LoadRulesetResult RulesetMatcher::CreateVerifiedMatcher(
    const RulesetSource& source,
    int expected_ruleset_checksum,
    std::unique_ptr<RulesetMatcher>* matcher) {
  DCHECK(matcher);
  DCHECK(IsAPIAvailable());

  base::ElapsedTimer timer;

  if (!base::PathExists(source.indexed_path()))
    return kLoadErrorInvalidPath;

  std::string ruleset_data;
  if (!base::ReadFileToString(source.indexed_path(), &ruleset_data))
    return kLoadErrorFileRead;

  if (!StripVersionHeaderAndParseVersion(&ruleset_data))
    return kLoadErrorVersionMismatch;

  // This guarantees that no memory access will end up outside the buffer.
  if (!IsValidRulesetData(
          base::make_span(reinterpret_cast<const uint8_t*>(ruleset_data.data()),
                          ruleset_data.size()),
          expected_ruleset_checksum)) {
    return kLoadErrorChecksumMismatch;
  }

  UMA_HISTOGRAM_TIMES(
      "Extensions.DeclarativeNetRequest.CreateVerifiedMatcherTime",
      timer.Elapsed());

  // Using WrapUnique instead of make_unique since this class has a private
  // constructor.
  *matcher = base::WrapUnique(new RulesetMatcher(
      std::move(ruleset_data), source.id(), source.priority()));
  return kLoadSuccess;
}

RulesetMatcher::~RulesetMatcher() = default;

bool RulesetMatcher::HasMatchingBlockRule(const RequestParams& params) const {
  DCHECK(params.url);
  return blocking_matcher_.FindMatch(
      *params.url, params.first_party_origin, params.element_type,
      flat_rule::ActivationType_NONE, params.is_third_party,
      kDisableGenericRules, FindRuleStrategy::kAny);
}

bool RulesetMatcher::HasMatchingAllowRule(const RequestParams& params) const {
  DCHECK(params.url);
  return allowing_matcher_.FindMatch(
      *params.url, params.first_party_origin, params.element_type,
      flat_rule::ActivationType_NONE, params.is_third_party,
      kDisableGenericRules, FindRuleStrategy::kAny);
}

bool RulesetMatcher::HasMatchingRedirectRule(const RequestParams& params,
                                             GURL* redirect_url) const {
  DCHECK(redirect_url);
  DCHECK(params.url);
  DCHECK_NE(flat_rule::ElementType_WEBSOCKET, params.element_type);

  // Retrieve the highest priority matching rule corresponding to the given
  // request parameters.
  const flat_rule::UrlRule* rule = redirect_matcher_.FindMatch(
      *params.url, params.first_party_origin, params.element_type,
      flat_rule::ActivationType_NONE, params.is_third_party,
      kDisableGenericRules, FindRuleStrategy::kHighestPriority);
  if (!rule)
    return false;

  // Find the UrlRuleMetadata corresponding to |rule|. Since |metadata_list_| is
  // sorted by rule id, use LookupByKey which binary searches for fast lookup.
  const flat::UrlRuleMetadata* metadata =
      metadata_list_->LookupByKey(rule->id());

  // There must be a UrlRuleMetadata object corresponding to each redirect rule.
  DCHECK(metadata);
  DCHECK_EQ(metadata->id(), rule->id());

  *redirect_url = GURL(base::StringPiece(metadata->redirect_url()->c_str(),
                                         metadata->redirect_url()->size()));
  DCHECK(redirect_url->is_valid());
  return true;
}

RulesetMatcher::RulesetMatcher(std::string ruleset_data,
                               size_t id,
                               size_t priority)
    : ruleset_data_(std::move(ruleset_data)),
      root_(flat::GetExtensionIndexedRuleset(ruleset_data_.data())),
      blocking_matcher_(root_->blocking_index()),
      allowing_matcher_(root_->allowing_index()),
      redirect_matcher_(root_->redirect_index()),
      metadata_list_(root_->extension_metadata()),
      id_(id),
      priority_(priority) {}

}  // namespace declarative_net_request
}  // namespace extensions
