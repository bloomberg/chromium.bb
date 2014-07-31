// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure stdint.h includes SIZE_MAX. (See C89, p259, footnote 221.)
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include "components/domain_reliability/config.h"

#include <stdint.h>

#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"

namespace {

bool ConvertURL(const base::StringPiece& string_piece, GURL* url) {
  *url = GURL(string_piece.as_string());
  return url->is_valid();
}

bool IsValidSampleRate(double p) { return p >= 0.0 && p <= 1.0; }

}  // namespace

namespace domain_reliability {

// static
const size_t DomainReliabilityConfig::kInvalidResourceIndex = SIZE_MAX;

DomainReliabilityConfig::Resource::Resource() {
}
DomainReliabilityConfig::Resource::~Resource() {}

bool DomainReliabilityConfig::Resource::MatchesUrl(const GURL& url) const {
  const std::string& spec = url.spec();

  ScopedVector<std::string>::const_iterator it;
  for (it = url_patterns.begin(); it != url_patterns.end(); it++) {
    if (MatchPattern(spec, **it))
      return true;
  }

  return false;
}

bool DomainReliabilityConfig::Resource::DecideIfShouldReportRequest(
    bool success) const {
  double sample_rate = success ? success_sample_rate : failure_sample_rate;
  DCHECK(IsValidSampleRate(sample_rate));
  return base::RandDouble() < sample_rate;
}

// static
void DomainReliabilityConfig::Resource::RegisterJSONConverter(
    base::JSONValueConverter<DomainReliabilityConfig::Resource>* converter) {
  converter->RegisterStringField("resource_name", &Resource::name);
  converter->RegisterRepeatedString("url_patterns", &Resource::url_patterns);
  converter->RegisterDoubleField("success_sample_rate",
                                 &Resource::success_sample_rate);
  converter->RegisterDoubleField("failure_sample_rate",
                                 &Resource::failure_sample_rate);
}

bool DomainReliabilityConfig::Resource::IsValid() const {
  return !name.empty() && !url_patterns.empty() &&
      IsValidSampleRate(success_sample_rate) &&
      IsValidSampleRate(failure_sample_rate);
}

DomainReliabilityConfig::Collector::Collector() {}
DomainReliabilityConfig::Collector::~Collector() {}

// static
void DomainReliabilityConfig::Collector::RegisterJSONConverter(
    base::JSONValueConverter<DomainReliabilityConfig::Collector>* converter) {
  converter->RegisterCustomField<GURL>("upload_url", &Collector::upload_url,
                                       &ConvertURL);
}

bool DomainReliabilityConfig::Collector::IsValid() const {
  return upload_url.is_valid();
}

DomainReliabilityConfig::DomainReliabilityConfig() : valid_until(0.0) {}
DomainReliabilityConfig::~DomainReliabilityConfig() {}

// static
scoped_ptr<const DomainReliabilityConfig> DomainReliabilityConfig::FromJSON(
    const base::StringPiece& json) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(json));
  base::JSONValueConverter<DomainReliabilityConfig> converter;
  DomainReliabilityConfig* config = new DomainReliabilityConfig();

  // If we can parse and convert the JSON into a valid config, return that.
  if (value && converter.Convert(*value, config) && config->IsValid())
    return scoped_ptr<const DomainReliabilityConfig>(config);
  else
    return scoped_ptr<const DomainReliabilityConfig>();
}

bool DomainReliabilityConfig::IsValid() const {
  if (valid_until == 0.0 || domain.empty() ||
      resources.empty() || collectors.empty()) {
    return false;
  }

  for (size_t i = 0; i < resources.size(); ++i) {
    if (!resources[i]->IsValid())
      return false;
  }

  for (size_t i = 0; i < collectors.size(); ++i) {
    if (!collectors[i]->IsValid())
      return false;
  }

  return true;
}

bool DomainReliabilityConfig::IsExpired(base::Time now) const {
  DCHECK_NE(0.0, valid_until);
  base::Time valid_until_time = base::Time::FromDoubleT(valid_until);
  return now > valid_until_time;
}

size_t DomainReliabilityConfig::GetResourceIndexForUrl(const GURL& url) const {
  // Removes username, password, and fragment.
  GURL sanitized_url = url.GetAsReferrer();

  for (size_t i = 0; i < resources.size(); ++i) {
    if (resources[i]->MatchesUrl(sanitized_url))
      return i;
  }

  return kInvalidResourceIndex;
}

// static
void DomainReliabilityConfig::RegisterJSONConverter(
    base::JSONValueConverter<DomainReliabilityConfig>* converter) {
  converter->RegisterStringField("config_version",
                                 &DomainReliabilityConfig::version);
  converter->RegisterDoubleField("config_valid_until",
                                 &DomainReliabilityConfig::valid_until);
  converter->RegisterStringField("monitored_domain",
                                 &DomainReliabilityConfig::domain);
  converter->RegisterRepeatedMessage("monitored_resources",
                                     &DomainReliabilityConfig::resources);
  converter->RegisterRepeatedMessage("collectors",
                                     &DomainReliabilityConfig::collectors);
}

}  // namespace domain_reliability
