// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_CONFIG_H_
#define COMPONENTS_DOMAIN_RELIABILITY_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_value_converter.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "url/gurl.h"

namespace domain_reliability {

// The per-origin configuration that controls which requests are measured and
// reported, with what frequency, and where the beacons are uploaded.
struct DOMAIN_RELIABILITY_EXPORT DomainReliabilityConfig {
 public:
  DomainReliabilityConfig();
  ~DomainReliabilityConfig();

  // Uses the JSONValueConverter to parse the JSON for a config into a struct.
  static std::unique_ptr<const DomainReliabilityConfig> FromJSON(
      const base::StringPiece& json);

  bool IsValid() const;

  double GetSampleRate(bool request_successful) const;

  // Registers with the JSONValueConverter so it will know how to convert the
  // JSON for a config into the struct.
  static void RegisterJSONConverter(
      base::JSONValueConverter<DomainReliabilityConfig>* converter);

  // TODO(chlily): Convert this to a url::Origin or just a domain name, since we
  // don't use the other components.
  GURL origin;
  bool include_subdomains;
  // Each entry in |collectors| must have scheme https.
  std::vector<std::unique_ptr<GURL>> collectors;

  double success_sample_rate;
  double failure_sample_rate;
  std::vector<std::unique_ptr<std::string>> path_prefixes;

 private:
  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityConfig);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_CONFIG_H_
