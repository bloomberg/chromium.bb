// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/safe_browsing_api_handler_util.h"

#include <stddef.h>

#include <string>

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
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
  UMA_THREAT_SUB_TYPE_LANDING = 1,
  UMA_THREAT_SUB_TYPE_DISTRIBUTION = 2,
  UMA_THREAT_SUB_TYPE_UNKNOWN = 3,
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

// Parse the extra key/value pair(s) added by Safe Browsing backend.  To make
// it binary compatible with the Pver3 metadata that UIManager expects to
// deserialize, we convert it to a MalwarePatternType.
//
// TODO(nparker):  When chrome desktop is converted to use Pver4, convert this
// to the new proto instead.
const std::string ParseExtraMetadataToPB(const base::DictionaryValue* match,
                                         SBThreatType threat_type) {
  std::string pattern_key;
  if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
    pattern_key = "pha_pattern_type";
  } else {
    DCHECK(threat_type == SB_THREAT_TYPE_URL_PHISHING);
    pattern_key = "se_pattern_type";
  }

  std::string pattern_type;
  if (!match->GetString(pattern_key, &pattern_type)) {
    ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_NOT_SET);
    return std::string();
  }

  MalwarePatternType pb;
  if (pattern_type == "LANDING") {
    pb.set_pattern_type(MalwarePatternType::LANDING);
    ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_LANDING);
  } else if (pattern_type == "DISTRIBUTION") {
    pb.set_pattern_type(MalwarePatternType::DISTRIBUTION);
    ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_DISTRIBUTION);
  } else {
    ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_UNKNOWN);
    return std::string();
  }

  return pb.SerializeAsString();
}

int GetThreatSeverity(int java_threat_num) {
  // Assign higher numbers to more severe threats.
  switch (java_threat_num) {
    case JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION:
      return 2;
    case JAVA_THREAT_TYPE_SOCIAL_ENGINEERING:
      return 1;
    default:
      // Unknown threat type
      return -1;
  }
}

SBThreatType JavaToSBThreatType(int java_threat_num) {
  switch (java_threat_num) {
    case JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION:
      return SB_THREAT_TYPE_URL_MALWARE;
    case JAVA_THREAT_TYPE_SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    default:
      // Unknown threat type
      return SB_THREAT_TYPE_SAFE;
  }
}

}  // namespace

// Valid examples:
// {"matches":[{"threat_type":"5"}]}
//   or
// {"matches":[{"threat_type":"4"},
//             {"threat_type":"5", "se_pattern_type":"LANDING"}]}
UmaRemoteCallResult ParseJsonToThreatAndPB(const std::string& metadata_str,
                                           SBThreatType* worst_threat,
                                           std::string* metadata_pb_str) {
  *worst_threat = SB_THREAT_TYPE_SAFE;  // Default to safe.
  *metadata_pb_str = std::string();

  if (metadata_str.empty())
    return UMA_STATUS_JSON_EMPTY;

  // Pick out the "matches" list.
  scoped_ptr<base::Value> value = base::JSONReader::Read(metadata_str);
  const base::ListValue* matches = nullptr;
  if (!value.get() || !value->IsType(base::Value::TYPE_DICTIONARY) ||
      !(static_cast<base::DictionaryValue*>(value.get()))
           ->GetList(kJsonKeyMatches, &matches) ||
      !matches) {
    return UMA_STATUS_JSON_FAILED_TO_PARSE;
  }

  // Go through each matched threat type and pick the most severe.
  int worst_threat_num = -1;
  const base::DictionaryValue* worst_match = nullptr;
  for (size_t i = 0; i < matches->GetSize(); i++) {
    // Get the threat number
    const base::DictionaryValue* match;
    std::string threat_num_str;
    int java_threat_num = -1;
    if (!matches->GetDictionary(i, &match) ||
        !match->GetString(kJsonKeyThreatType, &threat_num_str) ||
        !base::StringToInt(threat_num_str, &java_threat_num)) {
      continue;  // Skip malformed list entries
    }

    if (GetThreatSeverity(java_threat_num) >
        GetThreatSeverity(worst_threat_num)) {
      worst_threat_num = java_threat_num;
      worst_match = match;
    }
  }

  *worst_threat = JavaToSBThreatType(worst_threat_num);
  if (*worst_threat == SB_THREAT_TYPE_SAFE || !worst_match)
    return UMA_STATUS_JSON_UNKNOWN_THREAT;
  *metadata_pb_str = ParseExtraMetadataToPB(worst_match, *worst_threat);

  return UMA_STATUS_UNSAFE;  // success
}

}  // namespace safe_browsing
