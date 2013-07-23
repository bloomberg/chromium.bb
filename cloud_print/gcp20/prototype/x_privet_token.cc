// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/x_privet_token.h"

#include "base/base64.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace {

const char kXPrivetTokenDelimeter = ':';
const uint64 kTimeExpiration = 24*60*60;  // in seconds

}  // namespace

using base::Time;
using base::TimeDelta;

XPrivetToken::XPrivetToken() {
  UpdateSecret();
}

XPrivetToken::XPrivetToken(const std::string& secret,
                           const base::Time& gen_time)
    : secret_(secret),
      last_gen_time_(gen_time) {
}

std::string XPrivetToken::GenerateXToken() {
  if (Time::Now() > last_gen_time_ + TimeDelta::FromSeconds(kTimeExpiration))
    UpdateSecret();

  return GenerateXTokenWithTime(static_cast<uint64>(Time::Now().ToTimeT()));
}

bool XPrivetToken::CheckValidXToken(const std::string& token_encoded) const {
  std::string token;
  if (!base::Base64Decode(token_encoded, &token))
    return false;

  size_t delimeter_pos = token.find(kXPrivetTokenDelimeter);
  if (delimeter_pos == std::string::npos)
    return false;

  std::string issue_time_str = token.substr(delimeter_pos + 1);
  uint64 issue_time;
  if (!base::StringToUint64(issue_time_str, &issue_time))
    return false;

  if (GenerateXTokenWithTime(issue_time) != token_encoded)
    return false;

  return Time::FromTimeT(issue_time) - last_gen_time_ <
      TimeDelta::FromSeconds(kTimeExpiration);
}

std::string XPrivetToken::GenerateXTokenWithTime(uint64 issue_time) const {
  std::string result;
  std::string issue_time_str = base::StringPrintf("%"PRIu64, issue_time);
  std::string hash = base::SHA1HashString(secret_ +
                                          kXPrivetTokenDelimeter +
                                          issue_time_str);
  base::Base64Encode(hash + kXPrivetTokenDelimeter + issue_time_str, &result);
  return result;
}

void XPrivetToken::UpdateSecret() {
  secret_ = base::GenerateGUID();
  last_gen_time_ = base::Time::Now();
  VLOG(1) << "New Secret is Generated.";
}

