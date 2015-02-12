// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_seed_store.h"

#include "base/base64.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/compression_utils.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_variations {

namespace {

class TestVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit TestVariationsSeedStore(PrefService* local_state)
      : VariationsSeedStore(local_state) {}
  ~TestVariationsSeedStore() override {}

  bool StoreSeedForTesting(const std::string& seed_data) {
    return StoreSeedData(seed_data, std::string(), base::Time::Now(), NULL);
  }

  VariationsSeedStore::VerifySignatureResult VerifySeedSignature(
      const std::string& seed_bytes,
      const std::string& base64_seed_signature) override {
    return VariationsSeedStore::VARIATIONS_SEED_SIGNATURE_ENUM_SIZE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestVariationsSeedStore);
};


// Populates |seed| with simple test data. The resulting seed will contain one
// study called "test", which contains one experiment called "abc" with
// probability weight 100. |seed|'s study field will be cleared before adding
// the new study.
variations::VariationsSeed CreateTestSeed() {
  variations::VariationsSeed seed;
  variations::Study* study = seed.add_study();
  study->set_name("test");
  study->set_default_experiment_name("abc");
  variations::Study_Experiment* experiment = study->add_experiment();
  experiment->set_name("abc");
  experiment->set_probability_weight(100);
  seed.set_serial_number("123");
  return seed;
}

// Serializes |seed| to protobuf binary format.
std::string SerializeSeed(const variations::VariationsSeed& seed) {
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return serialized_seed;
}

// Compresses |data| using Gzip compression and returns the result.
std::string Compress(const std::string& data) {
  std::string compressed;
  const bool result = metrics::GzipCompress(data, &compressed);
  EXPECT_TRUE(result);
  return compressed;
}

// Serializes |seed| to compressed base64-encoded protobuf binary format.
std::string SerializeSeedBase64(const variations::VariationsSeed& seed) {
  std::string serialized_seed = SerializeSeed(seed);
  std::string base64_serialized_seed;
  base::Base64Encode(Compress(serialized_seed), &base64_serialized_seed);
  return base64_serialized_seed;
}

// Checks whether the pref with name |pref_name| is at its default value in
// |prefs|.
bool PrefHasDefaultValue(const TestingPrefServiceSimple& prefs,
                         const char* pref_name) {
  return prefs.FindPreference(pref_name)->IsDefaultValue();
}

}  // namespace

TEST(VariationsSeedStoreTest, LoadSeed) {
  // Store good seed data to test if loading from prefs works.
  const variations::VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);

  TestVariationsSeedStore seed_store(&prefs);

  variations::VariationsSeed loaded_seed;
  // Check that loading a seed works correctly.
  EXPECT_TRUE(seed_store.LoadSeed(&loaded_seed));

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  // Make sure the pref hasn't been changed.
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_EQ(base64_seed, prefs.GetString(prefs::kVariationsCompressedSeed));

  // Check that loading a bad seed returns false and clears the pref.
  prefs.SetString(prefs::kVariationsCompressedSeed, "this should fail");
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_FALSE(seed_store.LoadSeed(&loaded_seed));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));

  // Check that having no seed in prefs results in a return value of false.
  prefs.ClearPref(prefs::kVariationsCompressedSeed);
  EXPECT_FALSE(seed_store.LoadSeed(&loaded_seed));
}

TEST(VariationsSeedStoreTest, GetInvalidSignature) {
  const variations::VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSeed, base64_seed);

  // The below seed and signature pair were generated using the server's
  // private key.
  const std::string base64_seed_data =
      "CigxZDI5NDY0ZmIzZDc4ZmYxNTU2ZTViNTUxYzY0NDdjYmM3NGU1ZmQwEr0BCh9VTUEtVW5p"
      "Zm9ybWl0eS1UcmlhbC0xMC1QZXJjZW50GICckqUFOAFCB2RlZmF1bHRKCwoHZGVmYXVsdBAB"
      "SgwKCGdyb3VwXzAxEAFKDAoIZ3JvdXBfMDIQAUoMCghncm91cF8wMxABSgwKCGdyb3VwXzA0"
      "EAFKDAoIZ3JvdXBfMDUQAUoMCghncm91cF8wNhABSgwKCGdyb3VwXzA3EAFKDAoIZ3JvdXBf"
      "MDgQAUoMCghncm91cF8wORAB";
  const std::string base64_seed_signature =
      "MEQCIDD1IVxjzWYncun+9IGzqYjZvqxxujQEayJULTlbTGA/AiAr0oVmEgVUQZBYq5VLOSvy"
      "96JkMYgzTkHPwbv7K/CmgA==";
  const std::string base64_seed_signature_invalid =
      "AEQCIDD1IVxjzWYncun+9IGzqYjZvqxxujQEayJULTlbTGA/AiAr0oVmEgVUQZBYq5VLOSvy"
      "96JkMYgzTkHPwbv7K/CmgA==";

  // Set seed and valid signature in prefs.
  prefs.SetString(prefs::kVariationsSeed, base64_seed_data);
  prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);

  VariationsSeedStore seed_store(&prefs);
  variations::VariationsSeed loaded_seed;
  seed_store.LoadSeed(&loaded_seed);
  std::string invalid_signature = seed_store.GetInvalidSignature();
  // Valid signature so we get an empty string.
  EXPECT_EQ(std::string(), invalid_signature);

  prefs.SetString(prefs::kVariationsSeedSignature,
                  base64_seed_signature_invalid);
  seed_store.LoadSeed(&loaded_seed);
  // Invalid signature, so we should get the signature itself, except on mobile
  // where we should get an empty string because verification is not enabled.
  invalid_signature = seed_store.GetInvalidSignature();
#if defined(OS_IOS) || defined(OS_ANDROID)
  EXPECT_EQ(std::string(), invalid_signature);
#else
  EXPECT_EQ(base64_seed_signature_invalid, invalid_signature);
#endif

  prefs.SetString(prefs::kVariationsSeedSignature, std::string());
  seed_store.LoadSeed(&loaded_seed);
  invalid_signature = seed_store.GetInvalidSignature();
  // Empty signature, not considered invalid.
  EXPECT_EQ(std::string(), invalid_signature);
}

TEST(VariationsSeedStoreTest, StoreSeedData) {
  const variations::VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  TestVariationsSeedStore seed_store(&prefs);

  EXPECT_TRUE(seed_store.StoreSeedForTesting(serialized_seed));
  // Make sure the pref was actually set.
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));

  std::string loaded_compressed_seed =
      prefs.GetString(prefs::kVariationsCompressedSeed);
  std::string decoded_compressed_seed;
  ASSERT_TRUE(base::Base64Decode(loaded_compressed_seed,
                                 &decoded_compressed_seed));
  // Make sure the stored seed from pref is the same as the seed we created.
  EXPECT_EQ(Compress(serialized_seed), decoded_compressed_seed);

  // Check if trying to store a bad seed leaves the pref unchanged.
  prefs.ClearPref(prefs::kVariationsCompressedSeed);
  EXPECT_FALSE(seed_store.StoreSeedForTesting("should fail"));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
}

TEST(VariationsSeedStoreTest, StoreSeedData_ParsedSeed) {
  const variations::VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  variations::VariationsSeed parsed_seed;
  EXPECT_TRUE(seed_store.StoreSeedData(serialized_seed, std::string(),
                                       base::Time::Now(), &parsed_seed));
  EXPECT_EQ(serialized_seed, SerializeSeed(parsed_seed));
}

TEST(VariationsSeedStoreTest, VerifySeedSignature) {
  // The below seed and signature pair were generated using the server's
  // private key.
  const std::string base64_seed_data =
      "CigxZDI5NDY0ZmIzZDc4ZmYxNTU2ZTViNTUxYzY0NDdjYmM3NGU1ZmQwEr0BCh9VTUEtVW5p"
      "Zm9ybWl0eS1UcmlhbC0xMC1QZXJjZW50GICckqUFOAFCB2RlZmF1bHRKCwoHZGVmYXVsdBAB"
      "SgwKCGdyb3VwXzAxEAFKDAoIZ3JvdXBfMDIQAUoMCghncm91cF8wMxABSgwKCGdyb3VwXzA0"
      "EAFKDAoIZ3JvdXBfMDUQAUoMCghncm91cF8wNhABSgwKCGdyb3VwXzA3EAFKDAoIZ3JvdXBf"
      "MDgQAUoMCghncm91cF8wORAB";
  const std::string base64_seed_signature =
      "MEQCIDD1IVxjzWYncun+9IGzqYjZvqxxujQEayJULTlbTGA/AiAr0oVmEgVUQZBYq5VLOSvy"
      "96JkMYgzTkHPwbv7K/CmgA==";

  std::string seed_data;
  EXPECT_TRUE(base::Base64Decode(base64_seed_data, &seed_data));

  VariationsSeedStore seed_store(NULL);

#if defined(OS_IOS) || defined(OS_ANDROID)
  // Signature verification is not enabled on mobile.
  if (seed_store.VerifySeedSignature(seed_data, base64_seed_signature) ==
      VariationsSeedStore::VARIATIONS_SEED_SIGNATURE_ENUM_SIZE) {
    return;
  }
#endif

  // The above inputs should be valid.
  EXPECT_EQ(VariationsSeedStore::VARIATIONS_SEED_SIGNATURE_VALID,
            seed_store.VerifySeedSignature(seed_data, base64_seed_signature));

  // If there's no signature, the corresponding result should be returned.
  EXPECT_EQ(VariationsSeedStore::VARIATIONS_SEED_SIGNATURE_MISSING,
            seed_store.VerifySeedSignature(seed_data, std::string()));

  // Using non-base64 encoded value as signature (e.g. seed data) should fail.
  EXPECT_EQ(VariationsSeedStore::VARIATIONS_SEED_SIGNATURE_DECODE_FAILED,
            seed_store.VerifySeedSignature(seed_data, seed_data));

  // Using a different signature (e.g. the base64 seed data) should fail.
#if defined(USE_OPENSSL)
  // OpenSSL doesn't distinguish signature decode failure from the
  // signature not matching.
  EXPECT_EQ(VariationsSeedStore::VARIATIONS_SEED_SIGNATURE_INVALID_SEED,
            seed_store.VerifySeedSignature(seed_data, base64_seed_data));
#else
  EXPECT_EQ(VariationsSeedStore::VARIATIONS_SEED_SIGNATURE_INVALID_SIGNATURE,
            seed_store.VerifySeedSignature(seed_data, base64_seed_data));
#endif

  // Using a different seed should not match the signature.
  seed_data[0] = 'x';
  EXPECT_EQ(VariationsSeedStore::VARIATIONS_SEED_SIGNATURE_INVALID_SEED,
            seed_store.VerifySeedSignature(seed_data, base64_seed_signature));
}

}  // namespace chrome_variations
