// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SEED_STORE_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SEED_STORE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"

class PrefService;
class PrefRegistrySimple;

namespace variations {
class VariationsSeed;
}

namespace chrome_variations {

// VariationsSeedStore is a helper class for reading and writing the variations
// seed from Local State.
class VariationsSeedStore {
 public:
  explicit VariationsSeedStore(PrefService* local_state);
  virtual ~VariationsSeedStore();

  // Loads the variations seed data from local state into |seed|. If there is a
  // problem with loading, the pref value is cleared and false is returned. If
  // successful, |seed| will contain the loaded data and true is returned.
  bool LoadSeed(variations::VariationsSeed* seed);

  // Stores the given seed data (serialized protobuf data) to local state, along
  // with a base64-encoded digital signature for seed and the date when it was
  // fetched. The |seed_data| will be base64 encoded for storage. If the string
  // is invalid, the existing prefs are left as is and false is returned. On
  // success and if |parsed_seed| is not NULL, |parsed_seed| will be filled
  // with the de-serialized protobuf decoded from |seed_data|.
  bool StoreSeedData(const std::string& seed_data,
                     const std::string& base64_seed_signature,
                     const base::Time& date_fetched,
                     variations::VariationsSeed* parsed_seed);

  // Updates |kVariationsSeedDate| and logs when previous date was from a
  // different day.
  void UpdateSeedDateAndLogDayChange(const base::Time& server_date_fetched);

  // Returns the serial number of the last loaded or stored seed.
  const std::string& variations_serial_number() const {
    return variations_serial_number_;
  }

  // Registers Local State prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // Note: UMA histogram enum - don't re-order or remove entries.
  enum VerifySignatureResult {
    VARIATIONS_SEED_SIGNATURE_MISSING,
    VARIATIONS_SEED_SIGNATURE_DECODE_FAILED,
    VARIATIONS_SEED_SIGNATURE_INVALID_SIGNATURE,
    VARIATIONS_SEED_SIGNATURE_INVALID_SEED,
    VARIATIONS_SEED_SIGNATURE_VALID,
    VARIATIONS_SEED_SIGNATURE_ENUM_SIZE,
  };

  // Verifies a variations seed (the serialized proto bytes) with the specified
  // base-64 encoded signature that was received from the server and returns the
  // result. The signature is assumed to be an "ECDSA with SHA-256" signature
  // (see kECDSAWithSHA256AlgorithmID in the .cc file). Returns the result of
  // signature verification or VARIATIONS_SEED_SIGNATURE_ENUM_SIZE if signature
  // verification is not enabled.
  virtual VariationsSeedStore::VerifySignatureResult VerifySeedSignature(
      const std::string& seed_bytes,
      const std::string& base64_seed_signature);

 private:
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedStoreTest, VerifySeedSignature);

  // Clears all prefs related to variations seed storage.
  void ClearPrefs();

  // The pref service used to persist the variations seed.
  PrefService* local_state_;

  // Cached serial number from the most recently fetched variations seed.
  std::string variations_serial_number_;

  DISALLOW_COPY_AND_ASSIGN(VariationsSeedStore);
};

}  // namespace chrome_variations

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SEED_STORE_H_
