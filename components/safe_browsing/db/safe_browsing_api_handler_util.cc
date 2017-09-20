// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/safe_browsing_api_handler_util.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/safe_browsing_db/metadata.pb.h"
#include "components/safe_browsing_db/util.h"

namespace safe_browsing {
namespace {

// JSON metatdata keys. These are are fixed in the Java-side API.
const char kJsonKeyMatches[] = "matches";
const char kJsonKeyThreatType[] = "threat_type";

// Do not reorder or delete.  Make sure changes are reflected in
// SB2RemoteCallThreatSubType.
enum UmaThreatSubType {
  UMA_THREAT_SUB_TYPE_NOT_SET = 0,
  UMA_THREAT_SUB_TYPE_POTENTIALLY_HALMFUL_APP_LANDING = 1,
  UMA_THREAT_SUB_TYPE_POTENTIALLY_HALMFUL_APP_DISTRIBUTION = 2,
  UMA_THREAT_SUB_TYPE_UNKNOWN = 3,
  UMA_THREAT_SUB_TYPE_SOCIAL_ENGINEERING_ADS = 4,
  UMA_THREAT_SUB_TYPE_SOCIAL_ENGINEERING_LANDING = 5,
  UMA_THREAT_SUB_TYPE_PHISHING = 6,
  UMA_THREAT_SUB_TYPE_BETTER_ADS = 7,
  UMA_THREAT_SUB_TYPE_ABUSIVE_ADS = 8,
  UMA_THREAT_SUB_TYPE_ALL_ADS = 9,
  UMA_THREAT_SUB_TYPE_MAX_VALUE
};

void ReportUmaThreatSubType(SBThreatType threat_type,
                            UmaThreatSubType sub_type) {
  if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
    UMA_HISTOGRAM_ENUMERATION(
        "SB2.RemoteCall.ThreatSubType.PotentiallyHarmfulApp", sub_type,
        UMA_THREAT_SUB_TYPE_MAX_VALUE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("SB2.RemoteCall.ThreatSubType.SocialEngineering",
                              sub_type, UMA_THREAT_SUB_TYPE_MAX_VALUE);
  }
}

// Parse the appropriate "*_pattern_type" key from the metadata.
// Returns NONE if no pattern type was found.
ThreatPatternType ParseThreatSubType(const base::DictionaryValue* match,
                                     SBThreatType threat_type) {
  if (threat_type == SB_THREAT_TYPE_URL_UNWANTED)
    return ThreatPatternType::NONE;

  std::string pattern_key;
  switch (threat_type) {
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
      pattern_key = "sf_pattern_type";
      break;
    case SB_THREAT_TYPE_URL_MALWARE:
      pattern_key = "pha_pattern_type";
      break;
    case SB_THREAT_TYPE_URL_PHISHING:
      pattern_key = "se_pattern_type";
      break;
    default:
      NOTREACHED();
      break;
  }

  std::string pattern_type;
  if (!match->GetString(pattern_key, &pattern_type)) {
    ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_NOT_SET);
    return ThreatPatternType::NONE;
  }

  if (threat_type == SB_THREAT_TYPE_SUBRESOURCE_FILTER) {
    if (pattern_type == "BETTER_ADS") {
      ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_BETTER_ADS);
      return ThreatPatternType::SUBRESOURCE_FILTER_BETTER_ADS;
    } else if (pattern_type == "ABUSIVE_ADS") {
      ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_ABUSIVE_ADS);
      return ThreatPatternType::SUBRESOURCE_FILTER_ABUSIVE_ADS;
    } else if (pattern_type == "ALL_ADS") {
      ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_ALL_ADS);
      return ThreatPatternType::SUBRESOURCE_FILTER_ALL_ADS;
    } else {
      ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_UNKNOWN);
      return ThreatPatternType::NONE;
    }
  } else if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
    if (pattern_type == "LANDING") {
      ReportUmaThreatSubType(
          threat_type, UMA_THREAT_SUB_TYPE_POTENTIALLY_HALMFUL_APP_LANDING);
      return ThreatPatternType::MALWARE_LANDING;
    } else if (pattern_type == "DISTRIBUTION") {
      ReportUmaThreatSubType(
          threat_type,
          UMA_THREAT_SUB_TYPE_POTENTIALLY_HALMFUL_APP_DISTRIBUTION);
      return ThreatPatternType::MALWARE_DISTRIBUTION;
    } else {
      ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_UNKNOWN);
      return ThreatPatternType::NONE;
    }
  } else {
    DCHECK(threat_type == SB_THREAT_TYPE_URL_PHISHING);
    if (pattern_type == "SOCIAL_ENGINEERING_ADS") {
      ReportUmaThreatSubType(threat_type,
                             UMA_THREAT_SUB_TYPE_SOCIAL_ENGINEERING_ADS);
      return ThreatPatternType::SOCIAL_ENGINEERING_ADS;
    } else if (pattern_type == "SOCIAL_ENGINEERING_LANDING") {
      ReportUmaThreatSubType(threat_type,
                             UMA_THREAT_SUB_TYPE_SOCIAL_ENGINEERING_LANDING);
      return ThreatPatternType::SOCIAL_ENGINEERING_LANDING;
    } else if (pattern_type == "PHISHING") {
      ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_PHISHING);
      return ThreatPatternType::PHISHING;
    } else {
      ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_UNKNOWN);
      return ThreatPatternType::NONE;
    }
  }
}

// Parse the optional "UserPopulation" key from the metadata.
// Returns empty string if none was found.
std::string ParseUserPopulation(const base::DictionaryValue* match) {
  std::string population_id;
  if (!match->GetString("UserPopulation", &population_id))
    return std::string();
  else
    return population_id;
}

bool ParseExperimental(const base::DictionaryValue* match) {
  bool experimental = false;
  match->GetBoolean("experimental", &experimental);
  return experimental;
}

// Returns the severity level for a given SafeBrowsing list. The lowest value is
// 0, which represents the most severe list.
int GetThreatSeverity(JavaThreatTypes threat_type) {
  switch (threat_type) {
    case JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION:
      return 0;
    case JAVA_THREAT_TYPE_SOCIAL_ENGINEERING:
      return 1;
    case JAVA_THREAT_TYPE_UNWANTED_SOFTWARE:
      return 2;
    case JAVA_THREAT_TYPE_SUBRESOURCE_FILTER:
      return 3;
    case JAVA_THREAT_TYPE_MAX_VALUE:
      return std::numeric_limits<int>::max();
  }
  NOTREACHED() << "Unhandled threat_type: " << threat_type;
  return std::numeric_limits<int>::max();
}

SBThreatType JavaToSBThreatType(int java_threat_num) {
  switch (java_threat_num) {
    case JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION:
      return SB_THREAT_TYPE_URL_MALWARE;
    case JAVA_THREAT_TYPE_UNWANTED_SOFTWARE:
      return SB_THREAT_TYPE_URL_UNWANTED;
    case JAVA_THREAT_TYPE_SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    case JAVA_THREAT_TYPE_SUBRESOURCE_FILTER:
      return SB_THREAT_TYPE_SUBRESOURCE_FILTER;
    default:
      // Unknown threat type
      return SB_THREAT_TYPE_SAFE;
  }
}

}  // namespace

// Valid examples:
// {"matches":[{"threat_type":"5"}]}
//   or
// {"matches":[{"threat_type":"4", "pha_pattern_type":"LANDING"},
//             {"threat_type":"5"}]}
//   or
// {"matches":[{"threat_type":"4", "UserPopulation":"YXNvZWZpbmFqO..."}]
UmaRemoteCallResult ParseJsonFromGMSCore(const std::string& metadata_str,
                                         SBThreatType* worst_sb_threat_type,
                                         ThreatMetadata* metadata) {
  *worst_sb_threat_type = SB_THREAT_TYPE_SAFE;  // Default to safe.
  *metadata = ThreatMetadata();                 // Default values.

  if (metadata_str.empty())
    return UMA_STATUS_JSON_EMPTY;

  // Pick out the "matches" list.
  std::unique_ptr<base::Value> value = base::JSONReader::Read(metadata_str);
  const base::ListValue* matches = nullptr;
  if (!value.get() || !value->IsType(base::Value::Type::DICTIONARY) ||
      !(static_cast<base::DictionaryValue*>(value.get()))
           ->GetList(kJsonKeyMatches, &matches) ||
      !matches) {
    return UMA_STATUS_JSON_FAILED_TO_PARSE;
  }

  // Go through each matched threat type and pick the most severe.
  JavaThreatTypes worst_threat_type = JAVA_THREAT_TYPE_MAX_VALUE;
  const base::DictionaryValue* worst_match = nullptr;
  for (size_t i = 0; i < matches->GetSize(); i++) {
    // Get the threat number
    const base::DictionaryValue* match;
    std::string threat_num_str;

    int threat_type_num;
    if (!matches->GetDictionary(i, &match) ||
        !match->GetString(kJsonKeyThreatType, &threat_num_str) ||
        !base::StringToInt(threat_num_str, &threat_type_num)) {
      continue;  // Skip malformed list entries
    }

    JavaThreatTypes threat_type = static_cast<JavaThreatTypes>(threat_type_num);
    if (threat_type > JAVA_THREAT_TYPE_MAX_VALUE) {
      threat_type = JAVA_THREAT_TYPE_MAX_VALUE;
    }
    if (GetThreatSeverity(threat_type) < GetThreatSeverity(worst_threat_type)) {
      worst_threat_type = threat_type;
      worst_match = match;
    }
  }

  *worst_sb_threat_type = JavaToSBThreatType(worst_threat_type);
  if (*worst_sb_threat_type == SB_THREAT_TYPE_SAFE || !worst_match)
    return UMA_STATUS_JSON_UNKNOWN_THREAT;

  // Fill in the metadata
  metadata->threat_pattern_type =
      ParseThreatSubType(worst_match, *worst_sb_threat_type);
  metadata->population_id = ParseUserPopulation(worst_match);
  metadata->experimental = ParseExperimental(worst_match);

  return UMA_STATUS_MATCH;  // success
}

}  // namespace safe_browsing
