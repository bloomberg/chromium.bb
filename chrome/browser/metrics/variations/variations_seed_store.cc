// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_seed_store.h"

#include "base/base64.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "crypto/signature_verifier.h"

namespace chrome_variations {

namespace {

// Signature verification is disabled on mobile platforms for now, since it
// adds about ~15ms to the startup time on mobile (vs. a couple ms on desktop).
bool SignatureVerificationEnabled() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  return false;
#else
  return true;
#endif
}

// This is the algorithm ID for ECDSA with SHA-256. Parameters are ABSENT.
// RFC 5758:
//   ecdsa-with-SHA256 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
//        us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 2 }
//   ...
//   When the ecdsa-with-SHA224, ecdsa-with-SHA256, ecdsa-with-SHA384, or
//   ecdsa-with-SHA512 algorithm identifier appears in the algorithm field
//   as an AlgorithmIdentifier, the encoding MUST omit the parameters
//   field.  That is, the AlgorithmIdentifier SHALL be a SEQUENCE of one
//   component, the OID ecdsa-with-SHA224, ecdsa-with-SHA256, ecdsa-with-
//   SHA384, or ecdsa-with-SHA512.
// See also RFC 5480, Appendix A.
const uint8 kECDSAWithSHA256AlgorithmID[] = {
  0x30, 0x0a,
    0x06, 0x08,
      0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02,
};

// The ECDSA public key of the variations server for verifying variations seed
// signatures.
const uint8_t kPublicKey[] = {
  0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
  0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00,
  0x04, 0x51, 0x7c, 0x31, 0x4b, 0x50, 0x42, 0xdd, 0x59, 0xda, 0x0b, 0xfa, 0x43,
  0x44, 0x33, 0x7c, 0x5f, 0xa1, 0x0b, 0xd5, 0x82, 0xf6, 0xac, 0x04, 0x19, 0x72,
  0x6c, 0x40, 0xd4, 0x3e, 0x56, 0xe2, 0xa0, 0x80, 0xa0, 0x41, 0xb3, 0x23, 0x7b,
  0x71, 0xc9, 0x80, 0x87, 0xde, 0x35, 0x0d, 0x25, 0x71, 0x09, 0x7f, 0xb4, 0x15,
  0x2b, 0xff, 0x82, 0x4d, 0xd3, 0xfe, 0xc5, 0xef, 0x20, 0xc6, 0xa3, 0x10, 0xbf,
};

// Note: UMA histogram enum - don't re-order or remove entries.
enum VariationSeedEmptyState {
  VARIATIONS_SEED_NOT_EMPTY,
  VARIATIONS_SEED_EMPTY,
  VARIATIONS_SEED_CORRUPT,
  VARIATIONS_SEED_INVALID_SIGNATURE,
  VARIATIONS_SEED_EMPTY_ENUM_SIZE,
};

void RecordVariationSeedEmptyHistogram(VariationSeedEmptyState state) {
  UMA_HISTOGRAM_ENUMERATION("Variations.SeedEmpty", state,
                            VARIATIONS_SEED_EMPTY_ENUM_SIZE);
}

// Note: UMA histogram enum - don't re-order or remove entries.
enum VariationsSeedDateChangeState {
  SEED_DATE_NO_OLD_DATE,
  SEED_DATE_NEW_DATE_OLDER,
  SEED_DATE_SAME_DAY,
  SEED_DATE_NEW_DAY,
  SEED_DATE_ENUM_SIZE,
};

// Truncates a time to the start of the day in UTC. If given a time representing
// 2014-03-11 10:18:03.1 UTC, it will return a time representing
// 2014-03-11 00:00:00.0 UTC.
base::Time TruncateToUTCDay(const base::Time& time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  return base::Time::FromUTCExploded(exploded);
}

VariationsSeedDateChangeState GetSeedDateChangeState(
    const base::Time& server_seed_date,
    const base::Time& stored_seed_date) {
  if (server_seed_date < stored_seed_date)
    return SEED_DATE_NEW_DATE_OLDER;

  if (TruncateToUTCDay(server_seed_date) !=
      TruncateToUTCDay(stored_seed_date)) {
    // The server date is earlier than the stored date, and they are from
    // different UTC days, so |server_seed_date| is a valid new day.
    return SEED_DATE_NEW_DAY;
  }
  return SEED_DATE_SAME_DAY;
}

}  // namespace

VariationsSeedStore::VariationsSeedStore(PrefService* local_state)
    : local_state_(local_state) {
}

VariationsSeedStore::~VariationsSeedStore() {
}

bool VariationsSeedStore::LoadSeed(variations::VariationsSeed* seed) {
  const std::string base64_seed_data =
      local_state_->GetString(prefs::kVariationsSeed);
  if (base64_seed_data.empty()) {
    RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_EMPTY);
    return false;
  }

  // If the decode process fails, assume the pref value is corrupt and clear it.
  std::string seed_data;
  if (!base::Base64Decode(base64_seed_data, &seed_data) ||
      !seed->ParseFromString(seed_data)) {
    VLOG(1) << "Variations seed data in local pref is corrupt, clearing the "
            << "pref.";
    ClearPrefs();
    RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_CORRUPT);
    return false;
  }

  const std::string base64_seed_signature =
      local_state_->GetString(prefs::kVariationsSeedSignature);
  const VerifySignatureResult result =
      VerifySeedSignature(seed_data, base64_seed_signature);
  if (result != VARIATIONS_SEED_SIGNATURE_ENUM_SIZE) {
    UMA_HISTOGRAM_ENUMERATION("Variations.LoadSeedSignature", result,
                              VARIATIONS_SEED_SIGNATURE_ENUM_SIZE);
    if (result != VARIATIONS_SEED_SIGNATURE_VALID) {
      VLOG(1) << "Variations seed signature in local pref missing or invalid "
              << "with result: " << result << ". Clearing the pref.";
      ClearPrefs();
      RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_INVALID_SIGNATURE);
      return false;
    }
  }

  variations_serial_number_ = seed->serial_number();
  RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_NOT_EMPTY);
  return true;
}

bool VariationsSeedStore::StoreSeedData(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    const base::Time& date_fetched,
    variations::VariationsSeed* parsed_seed) {
  if (seed_data.empty()) {
    VLOG(1) << "Variations seed data is empty, rejecting the seed.";
    return false;
  }

  // Only store the seed data if it parses correctly.
  variations::VariationsSeed seed;
  if (!seed.ParseFromString(seed_data)) {
    VLOG(1) << "Variations seed data is not in valid proto format, "
            << "rejecting the seed.";
    return false;
  }

  const VerifySignatureResult result =
      VerifySeedSignature(seed_data, base64_seed_signature);
  if (result != VARIATIONS_SEED_SIGNATURE_ENUM_SIZE) {
    UMA_HISTOGRAM_ENUMERATION("Variations.StoreSeedSignature", result,
                              VARIATIONS_SEED_SIGNATURE_ENUM_SIZE);
    if (result != VARIATIONS_SEED_SIGNATURE_VALID) {
      VLOG(1) << "Variations seed signature missing or invalid with result: "
              << result << ". Rejecting the seed.";
      return false;
    }
  }

  std::string base64_seed_data;
  base::Base64Encode(seed_data, &base64_seed_data);

  // TODO(asvitkine): This pref is no longer being used. Remove it completely
  // in a couple of releases.
  local_state_->ClearPref(prefs::kVariationsSeedHash);

  local_state_->SetString(prefs::kVariationsSeed, base64_seed_data);
  UpdateSeedDateAndLogDayChange(date_fetched);
  local_state_->SetString(prefs::kVariationsSeedSignature,
                          base64_seed_signature);
  variations_serial_number_ = seed.serial_number();
  if (parsed_seed)
    seed.Swap(parsed_seed);

  return true;
}

void VariationsSeedStore::UpdateSeedDateAndLogDayChange(
    const base::Time& server_date_fetched) {
  VariationsSeedDateChangeState date_change = SEED_DATE_NO_OLD_DATE;

  if (local_state_->HasPrefPath(prefs::kVariationsSeedDate)) {
    const int64 stored_date_value =
        local_state_->GetInt64(prefs::kVariationsSeedDate);
    const base::Time stored_date =
        base::Time::FromInternalValue(stored_date_value);

    date_change = GetSeedDateChangeState(server_date_fetched, stored_date);
  }

  UMA_HISTOGRAM_ENUMERATION("Variations.SeedDateChange", date_change,
                            SEED_DATE_ENUM_SIZE);

  local_state_->SetInt64(prefs::kVariationsSeedDate,
                         server_date_fetched.ToInternalValue());
}

// static
void VariationsSeedStore::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kVariationsSeed, std::string());
  registry->RegisterStringPref(prefs::kVariationsSeedHash, std::string());
  registry->RegisterInt64Pref(prefs::kVariationsSeedDate,
                              base::Time().ToInternalValue());
  registry->RegisterStringPref(prefs::kVariationsSeedSignature, std::string());
}

void VariationsSeedStore::ClearPrefs() {
  local_state_->ClearPref(prefs::kVariationsSeed);
  local_state_->ClearPref(prefs::kVariationsSeedDate);
  local_state_->ClearPref(prefs::kVariationsSeedHash);
  local_state_->ClearPref(prefs::kVariationsSeedSignature);
}

VariationsSeedStore::VerifySignatureResult
VariationsSeedStore::VerifySeedSignature(
    const std::string& seed_bytes,
    const std::string& base64_seed_signature) {
  if (!SignatureVerificationEnabled())
    return VARIATIONS_SEED_SIGNATURE_ENUM_SIZE;

  if (base64_seed_signature.empty())
    return VARIATIONS_SEED_SIGNATURE_MISSING;

  std::string signature;
  if (!base::Base64Decode(base64_seed_signature, &signature))
    return VARIATIONS_SEED_SIGNATURE_DECODE_FAILED;

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(
          kECDSAWithSHA256AlgorithmID, sizeof(kECDSAWithSHA256AlgorithmID),
          reinterpret_cast<const uint8*>(signature.data()), signature.size(),
          kPublicKey, arraysize(kPublicKey))) {
    return VARIATIONS_SEED_SIGNATURE_INVALID_SIGNATURE;
  }

  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(seed_bytes.data()),
                        seed_bytes.size());
  if (verifier.VerifyFinal())
    return VARIATIONS_SEED_SIGNATURE_VALID;
  return VARIATIONS_SEED_SIGNATURE_INVALID_SEED;
}

}  // namespace chrome_variations
