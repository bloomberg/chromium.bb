// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_store.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "crypto/signature_verifier.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/zlib/google/compression_utils.h"

#if defined(OS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif  // OS_ANDROID

namespace variations {

namespace {

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

// Verifies a variations seed (the serialized proto bytes) with the specified
// base-64 encoded signature that was received from the server and returns the
// result. The signature is assumed to be an "ECDSA with SHA-256" signature
// (see kECDSAWithSHA256AlgorithmID in the .cc file). Returns the result of
// signature verification.
VerifySignatureResult VerifySeedSignature(
    const std::string& seed_bytes,
    const std::string& base64_seed_signature) {
  if (base64_seed_signature.empty())
    return VerifySignatureResult::MISSING_SIGNATURE;

  std::string signature;
  if (!base::Base64Decode(base64_seed_signature, &signature))
    return VerifySignatureResult::DECODE_FAILED;

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256,
                           reinterpret_cast<const uint8_t*>(signature.data()),
                           signature.size(), kPublicKey,
                           arraysize(kPublicKey))) {
    return VerifySignatureResult::INVALID_SIGNATURE;
  }

  verifier.VerifyUpdate(reinterpret_cast<const uint8_t*>(seed_bytes.data()),
                        seed_bytes.size());
  if (!verifier.VerifyFinal())
    return VerifySignatureResult::INVALID_SEED;

  return VerifySignatureResult::VALID_SIGNATURE;
}

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

  base::Time out_time;
  bool conversion_success = base::Time::FromUTCExploded(exploded, &out_time);
  DCHECK(conversion_success);
  return out_time;
}

UpdateSeedDateResult GetSeedDateChangeState(
    const base::Time& server_seed_date,
    const base::Time& stored_seed_date) {
  if (server_seed_date < stored_seed_date)
    return UpdateSeedDateResult::NEW_DATE_IS_OLDER;

  if (TruncateToUTCDay(server_seed_date) !=
      TruncateToUTCDay(stored_seed_date)) {
    // The server date is later than the stored date, and they are from
    // different UTC days, so |server_seed_date| is a valid new day.
    return UpdateSeedDateResult::NEW_DAY;
  }
  return UpdateSeedDateResult::SAME_DAY;
}

}  // namespace

VariationsSeedStore::VariationsSeedStore(PrefService* local_state)
    : local_state_(local_state) {}

VariationsSeedStore::~VariationsSeedStore() {
}

bool VariationsSeedStore::LoadSeed(VariationsSeed* seed,
                                   std::string* seed_data,
                                   std::string* base64_seed_signature) {
#if defined(OS_ANDROID)
  if (!local_state_->HasPrefPath(prefs::kVariationsSeedSignature))
    ImportFirstRunJavaSeed();
#endif  // OS_ANDROID

  LoadSeedResult read_result = ReadSeedData(seed_data);
  if (read_result != LoadSeedResult::SUCCESS) {
    RecordLoadSeedResult(read_result);
    return false;
  }

  *base64_seed_signature =
      local_state_->GetString(prefs::kVariationsSeedSignature);
  if (SignatureVerificationEnabled()) {
    const VerifySignatureResult result =
        VerifySeedSignature(*seed_data, *base64_seed_signature);
    UMA_HISTOGRAM_ENUMERATION("Variations.LoadSeedSignature", result,
                              VerifySignatureResult::ENUM_SIZE);
    if (result != VerifySignatureResult::VALID_SIGNATURE) {
      ClearPrefs();
      RecordLoadSeedResult(LoadSeedResult::INVALID_SIGNATURE);
      return false;
    }
  }

  if (!seed->ParseFromString(*seed_data)) {
    ClearPrefs();
    RecordLoadSeedResult(LoadSeedResult::CORRUPT_PROTOBUF);
    return false;
  }

  latest_serial_number_ = seed->serial_number();
  RecordLoadSeedResult(LoadSeedResult::SUCCESS);
  return true;
}

bool VariationsSeedStore::StoreSeedData(
    const std::string& data,
    const std::string& base64_seed_signature,
    const std::string& country_code,
    const base::Time& date_fetched,
    bool is_delta_compressed,
    bool is_gzip_compressed,
    bool fetched_insecurely,
    VariationsSeed* parsed_seed) {
  UMA_HISTOGRAM_BOOLEAN("Variations.StoreSeed.HasCountry",
                        !country_code.empty());

  // If the data is gzip compressed, first uncompress it.
  std::string ungzipped_data;
  if (is_gzip_compressed) {
    if (compression::GzipUncompress(data, &ungzipped_data)) {
      if (ungzipped_data.empty()) {
        RecordStoreSeedResult(StoreSeedResult::FAILED_EMPTY_GZIP_CONTENTS);
        return false;
      }

      int size_reduction = ungzipped_data.length() - data.length();
      UMA_HISTOGRAM_PERCENTAGE("Variations.StoreSeed.GzipSize.ReductionPercent",
                               100 * size_reduction / ungzipped_data.length());
      UMA_HISTOGRAM_COUNTS_1000("Variations.StoreSeed.GzipSize",
                                data.length() / 1024);
    } else {
      RecordStoreSeedResult(StoreSeedResult::FAILED_UNGZIP);
      return false;
    }
  } else {
    ungzipped_data = data;
  }

  if (!is_delta_compressed) {
    const bool result = StoreSeedDataNoDelta(
        ungzipped_data, base64_seed_signature, country_code, date_fetched,
        fetched_insecurely, parsed_seed);
    if (result) {
      UMA_HISTOGRAM_COUNTS_1000("Variations.StoreSeed.Size",
                                ungzipped_data.length() / 1024);
    }
    return result;
  }

  // If the data is delta compressed, first decode it.
  RecordStoreSeedResult(StoreSeedResult::DELTA_COUNT);

  std::string existing_seed_data;
  std::string updated_seed_data;
  LoadSeedResult read_result = ReadSeedData(&existing_seed_data);
  if (read_result != LoadSeedResult::SUCCESS) {
    RecordStoreSeedResult(StoreSeedResult::FAILED_DELTA_READ_SEED);
    return false;
  }
  if (!ApplyDeltaPatch(existing_seed_data, ungzipped_data,
                       &updated_seed_data)) {
    RecordStoreSeedResult(StoreSeedResult::FAILED_DELTA_APPLY);
    return false;
  }

  const bool result = StoreSeedDataNoDelta(
      updated_seed_data, base64_seed_signature, country_code, date_fetched,
      fetched_insecurely, parsed_seed);
  if (result) {
    // Note: |updated_seed_data.length()| is guaranteed to be non-zero, else
    // result would be false.
    int size_reduction = updated_seed_data.length() - ungzipped_data.length();
    UMA_HISTOGRAM_PERCENTAGE("Variations.StoreSeed.DeltaSize.ReductionPercent",
                             100 * size_reduction / updated_seed_data.length());
    UMA_HISTOGRAM_COUNTS_1000("Variations.StoreSeed.DeltaSize",
                              ungzipped_data.length() / 1024);
  } else {
    RecordStoreSeedResult(StoreSeedResult::FAILED_DELTA_STORE);
  }
  return result;
}

bool VariationsSeedStore::StoreSafeSeed(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    const ClientFilterableState& client_state) {
  std::string base64_seed_data;
  StoreSeedResult result = VerifyAndCompressSeedData(
      seed_data, base64_seed_signature, false /* fetched_insecurely */,
      SeedType::SAFE, &base64_seed_data, nullptr);
  UMA_HISTOGRAM_ENUMERATION("Variations.SafeMode.StoreSafeSeed.Result", result,
                            StoreSeedResult::ENUM_SIZE);
  if (result != StoreSeedResult::SUCCESS)
    return false;

  local_state_->SetString(prefs::kVariationsSafeCompressedSeed,
                          base64_seed_data);
  local_state_->SetString(prefs::kVariationsSafeSeedSignature,
                          base64_seed_signature);

  local_state_->SetTime(prefs::kVariationsSafeSeedDate,
                        client_state.reference_date);
  local_state_->SetString(prefs::kVariationsSafeSeedLocale,
                          client_state.locale);
  local_state_->SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry,
                          client_state.permanent_consistency_country);
  local_state_->SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry,
                          client_state.session_consistency_country);
  return true;
}

void VariationsSeedStore::UpdateSeedDateAndLogDayChange(
    const base::Time& server_date_fetched) {
  UpdateSeedDateResult result = UpdateSeedDateResult::NO_OLD_DATE;

  if (local_state_->HasPrefPath(prefs::kVariationsSeedDate)) {
    const base::Time stored_date =
        local_state_->GetTime(prefs::kVariationsSeedDate);
    result = GetSeedDateChangeState(server_date_fetched, stored_date);
  }

  UMA_HISTOGRAM_ENUMERATION("Variations.SeedDateChange", result,
                            UpdateSeedDateResult::ENUM_SIZE);

  local_state_->SetTime(prefs::kVariationsSeedDate, server_date_fetched);
}

const std::string& VariationsSeedStore::GetLatestSerialNumber() {
  if (latest_serial_number_.empty()) {
    // Efficiency note: This code should rarely be reached; typically, the
    // latest serial number should be cached via the call to LoadSeed(). The
    // call to ParseFromString() can be expensive, so it's best to only perform
    // it once, if possible: [ https://crbug.com/761684#c2 ]. At the time of
    // this writing, the only expected code path that should reach this code is
    // when running in safe mode.
    std::string seed_data;
    VariationsSeed seed;
    if (ReadSeedData(&seed_data) == LoadSeedResult::SUCCESS &&
        seed.ParseFromString(seed_data)) {
      latest_serial_number_ = seed.serial_number();
    }
  }
  return latest_serial_number_;
}

// static
void VariationsSeedStore::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kVariationsCompressedSeed, std::string());
  registry->RegisterStringPref(prefs::kVariationsCountry, std::string());
  registry->RegisterTimePref(prefs::kVariationsSeedDate, base::Time());
  registry->RegisterStringPref(prefs::kVariationsSeedSignature, std::string());

  // Safe seed prefs:
  registry->RegisterStringPref(prefs::kVariationsSafeCompressedSeed,
                               std::string());
  registry->RegisterTimePref(prefs::kVariationsSafeSeedDate, base::Time());
  registry->RegisterStringPref(prefs::kVariationsSafeSeedLocale, std::string());
  registry->RegisterStringPref(
      prefs::kVariationsSafeSeedPermanentConsistencyCountry, std::string());
  registry->RegisterStringPref(
      prefs::kVariationsSafeSeedSessionConsistencyCountry, std::string());
  registry->RegisterStringPref(prefs::kVariationsSafeSeedSignature,
                               std::string());
}

bool VariationsSeedStore::SignatureVerificationEnabled() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  // Signature verification is disabled on mobile platforms for now, since it
  // adds about ~15ms to the startup time on mobile (vs. a couple ms on
  // desktop).
  return false;
#else
  return true;
#endif
}

void VariationsSeedStore::ClearPrefs() {
  local_state_->ClearPref(prefs::kVariationsCompressedSeed);
  local_state_->ClearPref(prefs::kVariationsSeedDate);
  local_state_->ClearPref(prefs::kVariationsSeedSignature);
}

#if defined(OS_ANDROID)
void VariationsSeedStore::ImportFirstRunJavaSeed() {
  DVLOG(1) << "Importing first run seed from Java preferences.";

  std::string seed_data;
  std::string seed_signature;
  std::string seed_country;
  std::string response_date;
  bool is_gzip_compressed;

  android::GetVariationsFirstRunSeed(&seed_data, &seed_signature, &seed_country,
                                     &response_date, &is_gzip_compressed);
  if (seed_data.empty()) {
    RecordFirstRunSeedImportResult(
        FirstRunSeedImportResult::FAIL_NO_FIRST_RUN_SEED);
    return;
  }

  base::Time current_date;
  if (!base::Time::FromUTCString(response_date.c_str(), &current_date)) {
    RecordFirstRunSeedImportResult(
        FirstRunSeedImportResult::FAIL_INVALID_RESPONSE_DATE);
    LOG(WARNING) << "Invalid response date: " << response_date;
    return;
  }

  if (!StoreSeedData(seed_data, seed_signature, seed_country, current_date,
                     false, is_gzip_compressed, false, nullptr)) {
    RecordFirstRunSeedImportResult(FirstRunSeedImportResult::FAIL_STORE_FAILED);
    LOG(WARNING) << "First run variations seed is invalid.";
    return;
  }
  RecordFirstRunSeedImportResult(FirstRunSeedImportResult::SUCCESS);
}
#endif  // OS_ANDROID

LoadSeedResult VariationsSeedStore::ReadSeedData(std::string* seed_data) {
  std::string base64_seed_data =
      local_state_->GetString(prefs::kVariationsCompressedSeed);
  if (base64_seed_data.empty())
    return LoadSeedResult::EMPTY;

  // If the decode process fails, assume the pref value is corrupt and clear it.
  std::string decoded_data;
  if (!base::Base64Decode(base64_seed_data, &decoded_data)) {
    ClearPrefs();
    return LoadSeedResult::CORRUPT_BASE64;
  }

  if (!compression::GzipUncompress(decoded_data, seed_data)) {
    ClearPrefs();
    return LoadSeedResult::CORRUPT_GZIP;
  }

  return LoadSeedResult::SUCCESS;
}

bool VariationsSeedStore::StoreSeedDataNoDelta(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    const std::string& country_code,
    const base::Time& date_fetched,
    bool fetched_insecurely,
    VariationsSeed* parsed_seed) {
  std::string base64_seed_data;
  VariationsSeed seed;
  StoreSeedResult result = VerifyAndCompressSeedData(
      seed_data, base64_seed_signature, fetched_insecurely, SeedType::LATEST,
      &base64_seed_data, &seed);
  if (result != StoreSeedResult::SUCCESS) {
    RecordStoreSeedResult(result);
    return false;
  }

#if defined(OS_ANDROID)
  // If currently we do not have any stored pref then we mark seed storing as
  // successful on the Java side of Chrome for Android to avoid repeated seed
  // fetches and clear preferences on the Java side.
  if (local_state_->GetString(prefs::kVariationsCompressedSeed).empty()) {
    android::MarkVariationsSeedAsStored();
    android::ClearJavaFirstRunPrefs();
  }
#endif

  // Update the saved country code only if one was returned from the server.
  if (!country_code.empty())
    local_state_->SetString(prefs::kVariationsCountry, country_code);

  local_state_->SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
  UpdateSeedDateAndLogDayChange(date_fetched);
  local_state_->SetString(prefs::kVariationsSeedSignature,
                          base64_seed_signature);
  latest_serial_number_ = seed.serial_number();
  if (parsed_seed)
    seed.Swap(parsed_seed);

  RecordStoreSeedResult(StoreSeedResult::SUCCESS);
  return true;
}

StoreSeedResult VariationsSeedStore::VerifyAndCompressSeedData(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    bool fetched_insecurely,
    SeedType seed_type,
    std::string* base64_seed_data,
    VariationsSeed* parsed_seed) {
  if (seed_data.empty())
    return StoreSeedResult::FAILED_EMPTY_GZIP_CONTENTS;

  // Only store the seed data if it parses correctly.
  VariationsSeed seed;
  if (!seed.ParseFromString(seed_data))
    return StoreSeedResult::FAILED_PARSE;

  if (SignatureVerificationEnabled() || fetched_insecurely) {
    const VerifySignatureResult result =
        VerifySeedSignature(seed_data, base64_seed_signature);
    switch (seed_type) {
      case SeedType::LATEST:
        UMA_HISTOGRAM_ENUMERATION("Variations.StoreSeedSignature", result,
                                  VerifySignatureResult::ENUM_SIZE);
        break;
      case SeedType::SAFE:
        UMA_HISTOGRAM_ENUMERATION(
            "Variations.SafeMode.StoreSafeSeed.SignatureValidity", result,
            VerifySignatureResult::ENUM_SIZE);
        break;
    }

    if (result != VerifySignatureResult::VALID_SIGNATURE)
      return StoreSeedResult::FAILED_SIGNATURE;
  }

  // Compress the seed before base64-encoding and storing.
  std::string compressed_seed_data;
  if (!compression::GzipCompress(seed_data, &compressed_seed_data))
    return StoreSeedResult::FAILED_GZIP;

  base::Base64Encode(compressed_seed_data, base64_seed_data);
  if (parsed_seed)
    seed.Swap(parsed_seed);
  return StoreSeedResult::SUCCESS;
}

// static
bool VariationsSeedStore::ApplyDeltaPatch(const std::string& existing_data,
                                          const std::string& patch,
                                          std::string* output) {
  output->clear();

  google::protobuf::io::CodedInputStream in(
      reinterpret_cast<const uint8_t*>(patch.data()), patch.length());
  // Temporary string declared outside the loop so it can be re-used between
  // different iterations (rather than allocating new ones).
  std::string temp;

  const uint32_t existing_data_size =
      static_cast<uint32_t>(existing_data.size());
  while (in.CurrentPosition() != static_cast<int>(patch.length())) {
    uint32_t value;
    if (!in.ReadVarint32(&value))
      return false;

    if (value != 0) {
      // A non-zero value indicates the number of bytes to copy from the patch
      // stream to the output.

      // No need to guard against bad data (i.e. very large |value|) because the
      // call below will fail if |value| is greater than the size of the patch.
      if (!in.ReadString(&temp, value))
        return false;
      output->append(temp);
    } else {
      // Otherwise, when it's zero, it indicates that it's followed by a pair of
      // numbers - |offset| and |length| that specify a range of data to copy
      // from |existing_data|.
      uint32_t offset;
      uint32_t length;
      if (!in.ReadVarint32(&offset) || !in.ReadVarint32(&length))
        return false;

      // Check for |offset + length| being out of range and for overflow.
      base::CheckedNumeric<uint32_t> end_offset(offset);
      end_offset += length;
      if (!end_offset.IsValid() || end_offset.ValueOrDie() > existing_data_size)
        return false;
      output->append(existing_data, offset, length);
    }
  }
  return true;
}

void VariationsSeedStore::ReportUnsupportedSeedFormatError() {
  RecordStoreSeedResult(StoreSeedResult::FAILED_UNSUPPORTED_SEED_FORMAT);
}

}  // namespace variations
