// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_store.h"

#include "base/base64.h"
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

#if defined(OS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif  // OS_ANDROID

namespace variations {

namespace {

class TestVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit TestVariationsSeedStore(PrefService* local_state)
      : VariationsSeedStore(local_state) {}
  ~TestVariationsSeedStore() override {}

  bool StoreSeedForTesting(const std::string& seed_data) {
    return StoreSeedData(seed_data, std::string(), std::string(),
                         base::Time::Now(), false, false, nullptr);
  }

  bool SignatureVerificationEnabled() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestVariationsSeedStore);
};

// Signature verification is disabled on Android and iOS for performance
// reasons. This class re-enables it for tests, which don't mind the (small)
// performance penalty.
class SignatureVerifyingVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit SignatureVerifyingVariationsSeedStore(PrefService* local_state)
      : VariationsSeedStore(local_state) {}
  ~SignatureVerifyingVariationsSeedStore() override {}

  bool SignatureVerificationEnabled() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SignatureVerifyingVariationsSeedStore);
};

// Populates |seed| with simple test data. The resulting seed will contain one
// study called "test", which contains one experiment called "abc" with
// probability weight 100. |seed|'s study field will be cleared before adding
// the new study.
VariationsSeed CreateTestSeed() {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("test");
  study->set_default_experiment_name("abc");
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name("abc");
  experiment->set_probability_weight(100);
  seed.set_serial_number("123");
  return seed;
}

// Serializes |seed| to protobuf binary format.
std::string SerializeSeed(const VariationsSeed& seed) {
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return serialized_seed;
}

// Compresses |data| using Gzip compression and returns the result.
std::string Compress(const std::string& data) {
  std::string compressed;
  const bool result = compression::GzipCompress(data, &compressed);
  EXPECT_TRUE(result);
  return compressed;
}

// Serializes |seed| to compressed base64-encoded protobuf binary format.
std::string SerializeSeedBase64(const VariationsSeed& seed) {
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
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);

  TestVariationsSeedStore seed_store(&prefs);

  VariationsSeed loaded_seed;
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

TEST(VariationsSeedStoreTest, StoreSeedData) {
  const VariationsSeed seed = CreateTestSeed();
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
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  VariationsSeed parsed_seed;
  EXPECT_TRUE(seed_store.StoreSeedData(serialized_seed, std::string(),
                                       std::string(), base::Time::Now(), false,
                                       false, &parsed_seed));
  EXPECT_EQ(serialized_seed, SerializeSeed(parsed_seed));
}

TEST(VariationsSeedStoreTest, StoreSeedData_CountryCode) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  // Test with a valid header value.
  std::string seed = SerializeSeed(CreateTestSeed());
  EXPECT_TRUE(seed_store.StoreSeedData(seed, std::string(), "test_country",
                                       base::Time::Now(), false, false,
                                       nullptr));
  EXPECT_EQ("test_country", prefs.GetString(prefs::kVariationsCountry));

  // Test with no country code specified - which should preserve the old value.
  EXPECT_TRUE(seed_store.StoreSeedData(seed, std::string(), std::string(),
                                       base::Time::Now(), false, false,
                                       nullptr));
  EXPECT_EQ("test_country", prefs.GetString(prefs::kVariationsCountry));
}

TEST(VariationsSeedStoreTest, StoreSeedData_GzippedSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  std::string compressed_seed;
  ASSERT_TRUE(compression::GzipCompress(serialized_seed, &compressed_seed));

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  VariationsSeed parsed_seed;
  EXPECT_TRUE(seed_store.StoreSeedData(compressed_seed, std::string(),
                                       std::string(), base::Time::Now(), false,
                                       true, &parsed_seed));
  EXPECT_EQ(serialized_seed, SerializeSeed(parsed_seed));
}

TEST(VariationsSeedStoreTest, StoreSeedData_GzippedEmptySeed) {
  std::string empty_seed;
  std::string compressed_seed;
  ASSERT_TRUE(compression::GzipCompress(empty_seed, &compressed_seed));

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  VariationsSeed parsed_seed;
  EXPECT_FALSE(seed_store.StoreSeedData(compressed_seed, std::string(),
                                       std::string(), base::Time::Now(), false,
                                       true, &parsed_seed));
}

TEST(VariationsSeedStoreTest, VerifySeedSignature) {
  // The below seed and signature pair were generated using the server's
  // private key.
  const std::string uncompressed_base64_seed_data =
      "CigxZDI5NDY0ZmIzZDc4ZmYxNTU2ZTViNTUxYzY0NDdjYmM3NGU1ZmQwEr0BCh9VTUEtVW5p"
      "Zm9ybWl0eS1UcmlhbC0xMC1QZXJjZW50GICckqUFOAFCB2RlZmF1bHRKCwoHZGVmYXVsdBAB"
      "SgwKCGdyb3VwXzAxEAFKDAoIZ3JvdXBfMDIQAUoMCghncm91cF8wMxABSgwKCGdyb3VwXzA0"
      "EAFKDAoIZ3JvdXBfMDUQAUoMCghncm91cF8wNhABSgwKCGdyb3VwXzA3EAFKDAoIZ3JvdXBf"
      "MDgQAUoMCghncm91cF8wORAB";
  const std::string base64_seed_signature =
      "MEQCIDD1IVxjzWYncun+9IGzqYjZvqxxujQEayJULTlbTGA/AiAr0oVmEgVUQZBYq5VLOSvy"
      "96JkMYgzTkHPwbv7K/CmgA==";

  std::string seed_data;
  ASSERT_TRUE(base::Base64Decode(uncompressed_base64_seed_data, &seed_data));
  VariationsSeed seed;
  ASSERT_TRUE(seed.ParseFromString(seed_data));
  std::string base64_seed_data = SerializeSeedBase64(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  // The above inputs should be valid.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    EXPECT_TRUE(seed_store.LoadSeed(&seed));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::VALID_SIGNATURE),
        1);
  }

  // If there's no signature, the corresponding result should be returned.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, std::string());
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    EXPECT_FALSE(seed_store.LoadSeed(&seed));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::MISSING_SIGNATURE),
        1);
  }

  // Using non-base64 encoded value as signature should fail.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature,
                    "not a base64-encoded string");
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    EXPECT_FALSE(seed_store.LoadSeed(&seed));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::DECODE_FAILED),
        1);
  }

  // Using a different signature (e.g. the base64 seed data) should fail.
  // OpenSSL doesn't distinguish signature decode failure from the
  // signature not matching.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_data);
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    EXPECT_FALSE(seed_store.LoadSeed(&seed));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::INVALID_SEED),
        1);
  }

  // Using a different seed should not match the signature.
  {
    VariationsSeed wrong_seed;
    ASSERT_TRUE(wrong_seed.ParseFromString(seed_data));
    (*wrong_seed.mutable_study(0)->mutable_name())[0] = 'x';
    std::string base64_wrong_seed_data = SerializeSeedBase64(wrong_seed);

    prefs.SetString(prefs::kVariationsCompressedSeed, base64_wrong_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    EXPECT_FALSE(seed_store.LoadSeed(&seed));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::INVALID_SEED),
        1);
  }
}

TEST(VariationsSeedStoreTest, ApplyDeltaPatch) {
  // Sample seeds and the server produced delta between them to verify that the
  // client code is able to decode the deltas produced by the server.
  const std::string base64_before_seed_data =
      "CigxN2E4ZGJiOTI4ODI0ZGU3ZDU2MGUyODRlODY1ZDllYzg2NzU1MTE0ElgKDFVNQVN0YWJp"
      "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
      "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEkQKIFVNQS1Vbmlmb3JtaXR5LVRyaWFsLTEwMC1Q"
      "ZXJjZW50GIDjhcAFOAFCCGdyb3VwXzAxSgwKCGdyb3VwXzAxEAFgARJPCh9VTUEtVW5pZm9y"
      "bWl0eS1UcmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDAoIZ3JvdXBfMDEQAUoL"
      "CgdkZWZhdWx0EAFgAQ==";
  const std::string base64_after_seed_data =
      "CigyNGQzYTM3ZTAxYmViOWYwNWYzMjM4YjUzNWY3MDg1ZmZlZWI4NzQwElgKDFVNQVN0YWJp"
      "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
      "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEpIBCh9VTUEtVW5pZm9ybWl0eS1UcmlhbC0yMC1Q"
      "ZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKEQoIZ3JvdXBfMDEQARijtskBShEKCGdyb3VwXzAy"
      "EAEYpLbJAUoRCghncm91cF8wMxABGKW2yQFKEQoIZ3JvdXBfMDQQARimtskBShAKB2RlZmF1"
      "bHQQARiitskBYAESWAofVU1BLVVuaWZvcm1pdHktVHJpYWwtNTAtUGVyY2VudBiA44XABTgB"
      "QgdkZWZhdWx0Sg8KC25vbl9kZWZhdWx0EAFKCwoHZGVmYXVsdBABUgQoACgBYAE=";
  const std::string base64_delta_data =
      "KgooMjRkM2EzN2UwMWJlYjlmMDVmMzIzOGI1MzVmNzA4NWZmZWViODc0MAAqW+4BkgEKH1VN"
      "QS1Vbmlmb3JtaXR5LVRyaWFsLTIwLVBlcmNlbnQYgOOFwAU4AUIHZGVmYXVsdEoRCghncm91"
      "cF8wMRABGKO2yQFKEQoIZ3JvdXBfMDIQARiktskBShEKCGdyb3VwXzAzEAEYpbbJAUoRCghn"
      "cm91cF8wNBABGKa2yQFKEAoHZGVmYXVsdBABGKK2yQFgARJYCh9VTUEtVW5pZm9ybWl0eS1U"
      "cmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDwoLbm9uX2RlZmF1bHQQAUoLCgdk"
      "ZWZhdWx0EAFSBCgAKAFgAQ==";

  std::string before_seed_data;
  std::string after_seed_data;
  std::string delta_data;
  EXPECT_TRUE(base::Base64Decode(base64_before_seed_data, &before_seed_data));
  EXPECT_TRUE(base::Base64Decode(base64_after_seed_data, &after_seed_data));
  EXPECT_TRUE(base::Base64Decode(base64_delta_data, &delta_data));

  std::string output;
  EXPECT_TRUE(VariationsSeedStore::ApplyDeltaPatch(before_seed_data, delta_data,
                                                   &output));
  EXPECT_EQ(after_seed_data, output);
}

#if defined(OS_ANDROID)
TEST(VariationsSeedStoreTest, ImportFirstRunJavaSeed) {
  const std::string test_seed_data = "raw_seed_data_test";
  const std::string test_seed_signature = "seed_signature_test";
  const std::string test_seed_country = "seed_country_code_test";
  const std::string test_response_date = "seed_response_date_test";
  const bool test_is_gzip_compressed = true;
  android::SetJavaFirstRunPrefsForTesting(test_seed_data, test_seed_signature,
                                          test_seed_country, test_response_date,
                                          test_is_gzip_compressed);

  std::string seed_data;
  std::string seed_signature;
  std::string seed_country;
  std::string response_date;
  bool is_gzip_compressed;
  android::GetVariationsFirstRunSeed(&seed_data, &seed_signature, &seed_country,
                                     &response_date, &is_gzip_compressed);
  EXPECT_EQ(test_seed_data, seed_data);
  EXPECT_EQ(test_seed_signature, seed_signature);
  EXPECT_EQ(test_seed_country, seed_country);
  EXPECT_EQ(test_response_date, response_date);
  EXPECT_EQ(test_is_gzip_compressed, is_gzip_compressed);

  android::ClearJavaFirstRunPrefs();
  android::GetVariationsFirstRunSeed(&seed_data, &seed_signature, &seed_country,
                                     &response_date, &is_gzip_compressed);
  EXPECT_EQ("", seed_data);
  EXPECT_EQ("", seed_signature);
  EXPECT_EQ("", seed_country);
  EXPECT_EQ("", response_date);
  EXPECT_FALSE(is_gzip_compressed);
}
#endif  // OS_ANDROID

}  // namespace variations
