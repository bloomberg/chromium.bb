// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_code_generator.h"

#include <vector>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace chromeos {

ParentAccessCodeConfig::ParentAccessCodeConfig(
    const std::string& shared_secret,
    base::TimeDelta code_validity,
    base::TimeDelta clock_drift_tolerance)
    : shared_secret_(shared_secret),
      code_validity_(code_validity),
      clock_drift_tolerance_(clock_drift_tolerance) {
  DCHECK(!shared_secret_.empty());
  DCHECK(code_validity_ >= base::TimeDelta::FromSeconds(60));
  DCHECK(code_validity_ <= base::TimeDelta::FromMinutes(60));
  DCHECK(clock_drift_tolerance_ <= base::TimeDelta::FromMinutes(30));
}

ParentAccessCodeConfig::ParentAccessCodeConfig(
    const ParentAccessCodeConfig& rhs) = default;

ParentAccessCodeConfig& ParentAccessCodeConfig::operator=(
    const ParentAccessCodeConfig& rhs) = default;

ParentAccessCodeConfig::~ParentAccessCodeConfig() = default;

ParentAccessCode::ParentAccessCode(const std::string& code,
                                   base::Time valid_from,
                                   base::Time valid_to)
    : code_(code), valid_from_(valid_from), valid_to_(valid_to) {
  DCHECK_EQ(6u, code_.length());
  DCHECK_GT(valid_to_, valid_from_);
}

ParentAccessCode::ParentAccessCode(const ParentAccessCode&) = default;

ParentAccessCode& ParentAccessCode::operator=(const ParentAccessCode&) =
    default;

ParentAccessCode::~ParentAccessCode() = default;

bool ParentAccessCode::operator==(const ParentAccessCode& rhs) const {
  return code_ == rhs.code() && valid_from_ == rhs.valid_from() &&
         valid_to_ == rhs.valid_to();
}

bool ParentAccessCode::operator!=(const ParentAccessCode& rhs) const {
  return code_ != rhs.code() || valid_from_ != rhs.valid_from() ||
         valid_to_ != rhs.valid_to();
}

// static
constexpr base::TimeDelta ParentAccessCodeGenerator::kCodeGranularity;

ParentAccessCodeGenerator::ParentAccessCodeGenerator(
    const ParentAccessCodeConfig& config)
    : config_(config) {
  bool result = hmac_.Init(config_.shared_secret());
  DCHECK(result);
}

ParentAccessCodeGenerator::~ParentAccessCodeGenerator() = default;

base::Optional<ParentAccessCode> ParentAccessCodeGenerator::Generate(
    base::Time timestamp) {
  // We find the beginning of the interval for the given timestamp and adjust by
  // the granularity.
  const int64_t interval =
      timestamp.ToJavaTime() / config_.code_validity().InMilliseconds();
  const int64_t interval_beginning_timestamp =
      interval * config_.code_validity().InMilliseconds();
  const int64_t adjusted_timestamp =
      interval_beginning_timestamp / kCodeGranularity.InMilliseconds();

  // The algorithm for PAC generation is using data in Big-endian byte order to
  // feed HMAC.
  std::string big_endian_timestamp(sizeof(adjusted_timestamp), 0);
  base::WriteBigEndian(&big_endian_timestamp[0], adjusted_timestamp);

  std::vector<uint8_t> digest(hmac_.DigestLength());
  if (!hmac_.Sign(big_endian_timestamp, &digest[0], digest.size())) {
    LOG(ERROR) << "Signing HMAC data to generate Parent Access Code failed";
    return base::nullopt;
  }

  // Read 4 bytes in Big-endian order starting from |offset|.
  const int8_t offset = digest.back() & 0xf;
  int32_t result;
  std::vector<uint8_t> slice(digest.begin() + offset,
                             digest.begin() + offset + sizeof(result));
  base::ReadBigEndian(reinterpret_cast<char*>(slice.data()), &result);
  // Clear sign bit.
  result &= 0x7fffffff;

  const base::Time valid_from =
      base::Time::FromJavaTime(interval_beginning_timestamp);
  return ParentAccessCode(base::StringPrintf("%06d", result % 1000000),
                          valid_from, valid_from + config_.code_validity());
}

}  // namespace chromeos
