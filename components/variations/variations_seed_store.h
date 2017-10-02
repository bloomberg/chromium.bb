// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_STORE_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_STORE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/variations/metrics.h"

class PrefService;
class PrefRegistrySimple;

namespace variations {

class VariationsSeed;

// VariationsSeedStore is a helper class for reading and writing the variations
// seed from Local State.
class VariationsSeedStore {
 public:
  explicit VariationsSeedStore(PrefService* local_state);
  virtual ~VariationsSeedStore();

  // Loads the variations seed data from local state into |seed|. If there is a
  // problem with loading, the pref value is cleared and false is returned. If
  // successful, |seed| will contain the loaded data and true is returned.
  bool LoadSeed(VariationsSeed* seed) WARN_UNUSED_RESULT;

  // Stores the given seed |data| (serialized protobuf) to local state, along
  // with a base64-encoded digital signature for seed and the date when it was
  // fetched. If |is_gzip_compressed| is true, treats |data| as being gzip
  // compressed and decompresses it before any other processing.
  // If |is_delta_compressed| is true, treats |data| as being delta
  // compressed and attempts to decode it first using the store's seed data.
  // The actual seed data will be base64 encoded for storage. If the string
  // is invalid, the existing prefs are untouched and false is returned.
  // Additionally, stores the |country_code| that was received with the seed in
  // a separate pref. On success and if |parsed_seed| is not NULL, |parsed_seed|
  // will be filled with the de-serialized decoded protobuf.
  bool StoreSeedData(const std::string& data,
                     const std::string& base64_seed_signature,
                     const std::string& country_code,
                     const base::Time& date_fetched,
                     bool is_delta_compressed,
                     bool is_gzip_compressed,
                     VariationsSeed* parsed_seed) WARN_UNUSED_RESULT;

  // Updates |kVariationsSeedDate| and logs when previous date was from a
  // different day.
  void UpdateSeedDateAndLogDayChange(const base::Time& server_date_fetched);

  // Reports to UMA that the seed format specified by the server is unsupported.
  void ReportUnsupportedSeedFormatError();

  // Returns the serial number of the last loaded or stored seed.
  const std::string& variations_serial_number() const {
    return variations_serial_number_;
  }

  // Registers Local State prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  PrefService* local_state() { return local_state_; }

  const PrefService* local_state() const { return local_state_; }

 protected:
  // Whether signature verification is enabled. Overridable for tests.
  virtual bool SignatureVerificationEnabled();

 private:
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedStoreTest, VerifySeedSignature);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedStoreTest, ApplyDeltaPatch);

  // Clears all prefs related to variations seed storage.
  void ClearPrefs();

#if defined(OS_ANDROID)
  // Imports the variations seed data from Java side during the first
  // Chrome for Android run.
  void ImportFirstRunJavaSeed();
#endif  // OS_ANDROID

  // Reads the variations seed data from prefs into |seed_data|, and returns the
  // result of the load. The value stored into |seed_data| should only be used
  // if the result is SUCCESS.
  // Side-effect: If the read fails, clears the prefs associated with the seed.
  LoadSeedResult ReadSeedData(std::string* seed_data) WARN_UNUSED_RESULT;

  // Internal version of |StoreSeedData()| that assumes |seed_data| is not delta
  // compressed.
  bool StoreSeedDataNoDelta(const std::string& seed_data,
                            const std::string& base64_seed_signature,
                            const std::string& country_code,
                            const base::Time& date_fetched,
                            VariationsSeed* parsed_seed) WARN_UNUSED_RESULT;

  // Applies a delta-compressed |patch| to |existing_data|, producing the result
  // in |output|. Returns whether the operation was successful.
  static bool ApplyDeltaPatch(const std::string& existing_data,
                              const std::string& patch,
                              std::string* output) WARN_UNUSED_RESULT;

  // The pref service used to persist the variations seed.
  PrefService* local_state_;

  // Cached serial number from the most recently fetched variations seed.
  std::string variations_serial_number_;

  DISALLOW_COPY_AND_ASSIGN(VariationsSeedStore);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_STORE_H_
