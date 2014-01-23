// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_seed_store.h"

#include <set>

#include "base/base64.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace chrome_variations {

namespace {

// Computes a hash of the serialized variations seed data.
std::string HashSeed(const std::string& seed_data) {
  const std::string sha1 = base::SHA1HashString(seed_data);
  return base::HexEncode(sha1.data(), sha1.size());
}

enum VariationSeedEmptyState {
  VARIATIONS_SEED_NOT_EMPTY,
  VARIATIONS_SEED_EMPTY,
  VARIATIONS_SEED_CORRUPT,
  VARIATIONS_SEED_EMPTY_ENUM_SIZE,
};

void RecordVariationSeedEmptyHistogram(VariationSeedEmptyState state) {
  UMA_HISTOGRAM_ENUMERATION("Variations.SeedEmpty", state,
                            VARIATIONS_SEED_EMPTY_ENUM_SIZE);
}

}  // namespace

VariationsSeedStore::VariationsSeedStore(PrefService* local_state)
    : local_state_(local_state) {
}

VariationsSeedStore::~VariationsSeedStore() {
}

bool VariationsSeedStore::LoadSeed(VariationsSeed* seed) {
  const std::string base64_seed_data =
      local_state_->GetString(prefs::kVariationsSeed);
  if (base64_seed_data.empty()) {
    RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_EMPTY);
    return false;
  }

  const std::string hash_from_pref =
      local_state_->GetString(prefs::kVariationsSeedHash);
  // If the decode process fails, assume the pref value is corrupt and clear it.
  std::string seed_data;
  if (!base::Base64Decode(base64_seed_data, &seed_data) ||
      (!hash_from_pref.empty() && HashSeed(seed_data) != hash_from_pref) ||
      !seed->ParseFromString(seed_data)) {
    VLOG(1) << "Variations seed data in local pref is corrupt, clearing the "
            << "pref.";
    ClearPrefs();
    RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_CORRUPT);
    return false;
  }
  variations_serial_number_ = seed->serial_number();
  RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_NOT_EMPTY);
  return true;
}

bool VariationsSeedStore::StoreSeedData(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    const base::Time& date_fetched) {
  if (seed_data.empty()) {
    VLOG(1) << "Variations seed data is empty, rejecting the seed.";
    return false;
  }

  // Only store the seed data if it parses correctly.
  VariationsSeed seed;
  if (!seed.ParseFromString(seed_data)) {
    VLOG(1) << "Variations seed data is not in valid proto format, "
            << "rejecting the seed.";
    return false;
  }

  std::string base64_seed_data;
  base::Base64Encode(seed_data, &base64_seed_data);

  local_state_->SetString(prefs::kVariationsSeed, base64_seed_data);
  local_state_->SetString(prefs::kVariationsSeedHash, HashSeed(seed_data));
  local_state_->SetInt64(prefs::kVariationsSeedDate,
                         date_fetched.ToInternalValue());
  local_state_->SetString(prefs::kVariationsSeedSignature,
                          base64_seed_signature);
  variations_serial_number_ = seed.serial_number();

  return true;
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

}  // namespace chrome_variations
