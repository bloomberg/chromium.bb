// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/config.h"

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

DomainReliabilityConfig::Resource::Resource() {}

DomainReliabilityConfig::Resource::~Resource() {}

bool DomainReliabilityConfig::Resource::MatchesUrlString(
    const std::string& url_string) const {
  ScopedVector<std::string>::const_iterator it;

  for (it = url_patterns.begin(); it != url_patterns.end(); it++) {
    if (MatchPattern(url_string, **it))
      return true;
  }

  return false;
}

bool DomainReliabilityConfig::Resource::DecideIfShouldReportRequest(
    bool success) const {
  double sample_rate = success ? success_sample_rate : failure_sample_rate;
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
  base::Value* value = base::JSONReader::Read(json);
  if (!value)
    return scoped_ptr<const DomainReliabilityConfig>();

  DomainReliabilityConfig* config = new DomainReliabilityConfig();
  base::JSONValueConverter<DomainReliabilityConfig> converter;
  if (!converter.Convert(*value, config)) {
    return scoped_ptr<const DomainReliabilityConfig>();
  }

  if (!config->IsValid())
    return scoped_ptr<const DomainReliabilityConfig>();

  return scoped_ptr<const DomainReliabilityConfig>(config);
}

bool DomainReliabilityConfig::IsValid() const {
  if (valid_until == 0.0 || domain.empty() || resources.empty() ||
      collectors.empty()) {
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
  return now < base::Time::FromDoubleT(valid_until);
}

int DomainReliabilityConfig::GetResourceIndexForUrl(const GURL& url) const {
  const std::string& url_string = url.spec();

  for (size_t i = 0; i < resources.size(); ++i) {
    if (resources[i]->MatchesUrlString(url_string))
      return static_cast<int>(i);
  }

  return -1;
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
