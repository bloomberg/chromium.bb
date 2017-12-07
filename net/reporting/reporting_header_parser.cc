// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_header_parser.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_client.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"

namespace net {

namespace {

enum class HeaderOutcome {
  DISCARDED_NO_REPORTING_SERVICE = 0,
  DISCARDED_INVALID_SSL_INFO = 1,
  DISCARDED_CERT_STATUS_ERROR = 2,
  DISCARDED_INVALID_JSON = 3,
  PARSED = 4,

  MAX
};

void RecordHeaderOutcome(HeaderOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Reporting.HeaderOutcome", outcome,
                            HeaderOutcome::MAX);
}

enum class HeaderEndpointOutcome {
  DISCARDED_NOT_DICTIONARY = 0,
  DISCARDED_ENDPOINT_MISSING = 1,
  DISCARDED_ENDPOINT_NOT_STRING = 2,
  DISCARDED_ENDPOINT_INVALID = 3,
  DISCARDED_ENDPOINT_INSECURE = 4,
  DISCARDED_TTL_MISSING = 5,
  DISCARDED_TTL_NOT_INTEGER = 6,
  DISCARDED_TTL_NEGATIVE = 7,
  DISCARDED_GROUP_NOT_STRING = 8,
  REMOVED = 9,
  SET_REJECTED_BY_DELEGATE = 10,
  SET = 11,

  DISCARDED_PRIORITY_NOT_INTEGER = 12,
  DISCARDED_WEIGHT_NOT_INTEGER = 13,
  DISCARDED_WEIGHT_NOT_POSITIVE = 14,

  MAX
};

bool EndpointParsedSuccessfully(HeaderEndpointOutcome outcome) {
  return outcome == HeaderEndpointOutcome::REMOVED ||
         outcome == HeaderEndpointOutcome::SET_REJECTED_BY_DELEGATE ||
         outcome == HeaderEndpointOutcome::SET;
}

void RecordHeaderEndpointOutcome(HeaderEndpointOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Reporting.HeaderEndpointOutcome", outcome,
                            HeaderEndpointOutcome::MAX);
}

const char kUrlKey[] = "url";
const char kIncludeSubdomainsKey[] = "includeSubdomains";
const char kGroupKey[] = "group";
const char kGroupDefaultValue[] = "default";
const char kMaxAgeKey[] = "max-age";
const char kPriorityKey[] = "priority";
const char kWeightKey[] = "weight";

// Processes a single endpoint tuple received in a Report-To header.
//
// |origin| is the origin that sent the Report-To header.
//
// |value| is the parsed JSON value of the endpoint tuple.
//
// |*endpoint_out| will contain the endpoint URL parsed out of the tuple.
HeaderEndpointOutcome ProcessEndpoint(ReportingDelegate* delegate,
                                      ReportingCache* cache,
                                      base::TimeTicks now,
                                      const url::Origin& origin,
                                      const base::Value& value,
                                      GURL* endpoint_url_out) {
  *endpoint_url_out = GURL();

  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict))
    return HeaderEndpointOutcome::DISCARDED_NOT_DICTIONARY;
  DCHECK(dict);

  std::string endpoint_url_string;
  if (!dict->HasKey(kUrlKey))
    return HeaderEndpointOutcome::DISCARDED_ENDPOINT_MISSING;
  if (!dict->GetString(kUrlKey, &endpoint_url_string))
    return HeaderEndpointOutcome::DISCARDED_ENDPOINT_NOT_STRING;

  GURL endpoint_url(endpoint_url_string);
  if (!endpoint_url.is_valid())
    return HeaderEndpointOutcome::DISCARDED_ENDPOINT_INVALID;
  if (!endpoint_url.SchemeIsCryptographic())
    return HeaderEndpointOutcome::DISCARDED_ENDPOINT_INSECURE;

  int ttl_sec = -1;
  if (!dict->HasKey(kMaxAgeKey))
    return HeaderEndpointOutcome::DISCARDED_TTL_MISSING;
  if (!dict->GetInteger(kMaxAgeKey, &ttl_sec))
    return HeaderEndpointOutcome::DISCARDED_TTL_NOT_INTEGER;
  if (ttl_sec < 0)
    return HeaderEndpointOutcome::DISCARDED_TTL_NEGATIVE;

  std::string group = kGroupDefaultValue;
  if (dict->HasKey(kGroupKey) && !dict->GetString(kGroupKey, &group))
    return HeaderEndpointOutcome::DISCARDED_GROUP_NOT_STRING;

  ReportingClient::Subdomains subdomains = ReportingClient::Subdomains::EXCLUDE;
  bool subdomains_bool = false;
  if (dict->HasKey(kIncludeSubdomainsKey) &&
      dict->GetBoolean(kIncludeSubdomainsKey, &subdomains_bool) &&
      subdomains_bool == true) {
    subdomains = ReportingClient::Subdomains::INCLUDE;
  }

  int priority = ReportingClient::kDefaultPriority;
  if (dict->HasKey(kPriorityKey) && !dict->GetInteger(kPriorityKey, &priority))
    return HeaderEndpointOutcome::DISCARDED_PRIORITY_NOT_INTEGER;

  int weight = ReportingClient::kDefaultWeight;
  if (dict->HasKey(kWeightKey) && !dict->GetInteger(kWeightKey, &weight))
    return HeaderEndpointOutcome::DISCARDED_WEIGHT_NOT_INTEGER;
  if (weight <= 0)
    return HeaderEndpointOutcome::DISCARDED_WEIGHT_NOT_POSITIVE;

  *endpoint_url_out = endpoint_url;

  if (ttl_sec == 0) {
    cache->RemoveClientForOriginAndEndpoint(origin, endpoint_url);
    return HeaderEndpointOutcome::REMOVED;
  }

  if (!delegate->CanSetClient(origin, endpoint_url))
    return HeaderEndpointOutcome::SET_REJECTED_BY_DELEGATE;

  cache->SetClient(origin, endpoint_url, subdomains, group,
                   now + base::TimeDelta::FromSeconds(ttl_sec), priority,
                   weight);
  return HeaderEndpointOutcome::SET;
}

}  // namespace

// static
void ReportingHeaderParser::RecordHeaderDiscardedForNoReportingService() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_NO_REPORTING_SERVICE);
}

// static
void ReportingHeaderParser::RecordHeaderDiscardedForInvalidSSLInfo() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_INVALID_SSL_INFO);
}

// static
void ReportingHeaderParser::RecordHeaderDiscardedForCertStatusError() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_CERT_STATUS_ERROR);
}

// static
void ReportingHeaderParser::ParseHeader(ReportingContext* context,
                                        const GURL& url,
                                        const std::string& json_value) {
  DCHECK(url.SchemeIsCryptographic());

  std::unique_ptr<base::Value> value =
      base::JSONReader::Read("[" + json_value + "]");
  if (!value) {
    RecordHeaderOutcome(HeaderOutcome::DISCARDED_INVALID_JSON);
    return;
  }

  const base::ListValue* endpoint_list = nullptr;
  bool is_list = value->GetAsList(&endpoint_list);
  DCHECK(is_list);

  ReportingDelegate* delegate = context->delegate();
  ReportingCache* cache = context->cache();

  url::Origin origin = url::Origin::Create(url);

  std::vector<GURL> old_endpoints;
  cache->GetEndpointsForOrigin(origin, &old_endpoints);

  std::set<GURL> new_endpoints;

  base::TimeTicks now = context->tick_clock()->NowTicks();
  for (size_t i = 0; i < endpoint_list->GetSize(); i++) {
    const base::Value* endpoint = nullptr;
    bool got_endpoint = endpoint_list->Get(i, &endpoint);
    DCHECK(got_endpoint);
    GURL endpoint_url;
    HeaderEndpointOutcome outcome =
        ProcessEndpoint(delegate, cache, now, origin, *endpoint, &endpoint_url);
    if (EndpointParsedSuccessfully(outcome))
      new_endpoints.insert(endpoint_url);
    RecordHeaderEndpointOutcome(outcome);
  }

  // Remove any endpoints that weren't specified in the current header(s).
  for (const GURL& old_endpoint : old_endpoints) {
    if (new_endpoints.count(old_endpoint) == 0u)
      cache->RemoveClientForOriginAndEndpoint(origin, old_endpoint);
  }
}

}  // namespace net
