// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_service.h"

#include <vector>

#include "base/base64.h"
#include "base/prefs/testing_pref_service.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/web_resource/resource_request_allowed_notifier_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#endif

namespace chrome_variations {

namespace {

// A test class used to validate expected functionality in VariationsService.
class TestVariationsService : public VariationsService {
 public:
  TestVariationsService(TestRequestAllowedNotifier* test_notifier,
                        PrefService* local_state)
      : VariationsService(test_notifier, local_state),
        fetch_attempted_(false) {
    // Set this so StartRepeatedVariationsSeedFetch can be called in tests.
    SetCreateTrialsFromSeedCalledForTesting(true);
  }

  virtual ~TestVariationsService() {
  }

  bool fetch_attempted() const { return fetch_attempted_; }

 protected:
  virtual void DoActualFetch() OVERRIDE {
    fetch_attempted_ = true;
  }

 private:
  bool fetch_attempted_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsService);
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

// Serializes |seed| to base64-encoded protobuf binary format.
std::string SerializeSeedBase64(const VariationsSeed& seed, std::string* hash) {
  std::string serialized_seed = SerializeSeed(seed);
  if (hash != NULL) {
    std::string sha1 = base::SHA1HashString(serialized_seed);
    *hash = base::HexEncode(sha1.data(), sha1.size());
  }
  std::string base64_serialized_seed;
  EXPECT_TRUE(base::Base64Encode(serialized_seed, &base64_serialized_seed));
  return base64_serialized_seed;
}

// Simulates a variations service response by setting a date header and the
// specified HTTP |response_code| on |fetcher|.
void SimulateServerResponse(int response_code, net::TestURLFetcher* fetcher) {
  ASSERT_TRUE(fetcher);
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("date:Wed, 13 Feb 2013 00:25:24 GMT\0\0"));
  fetcher->set_response_headers(headers);
  fetcher->set_response_code(response_code);
}

}  // namespace

class VariationsServiceTest : public ::testing::Test {
 protected:
  VariationsServiceTest() {}

 private:
#if defined(OS_CHROMEOS)
  // Not used directly. Initializes CrosSettings for testing.
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
#endif

  DISALLOW_COPY_AND_ASSIGN(VariationsServiceTest);
};

#if !defined(OS_CHROMEOS)
TEST_F(VariationsServiceTest, VariationsURLIsValid) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  const std::string default_variations_url =
      VariationsService::GetDefaultVariationsServerURLForTesting();

  std::string value;
  GURL url = VariationsService::GetVariationsServerURL(&prefs);
  EXPECT_TRUE(StartsWithASCII(url.spec(), default_variations_url, true));
  EXPECT_FALSE(net::GetValueForKeyInQuery(url, "restrict", &value));

  prefs.SetString(prefs::kVariationsRestrictParameter, "restricted");
  url = VariationsService::GetVariationsServerURL(&prefs);
  EXPECT_TRUE(StartsWithASCII(url.spec(), default_variations_url, true));
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("restricted", value);
}
#else
class VariationsServiceTestChromeOS : public VariationsServiceTest {
 protected:
  VariationsServiceTestChromeOS() {}

  virtual void SetUp() OVERRIDE {
    cros_settings_ = chromeos::CrosSettings::Get();
    DCHECK(cros_settings_ != NULL);
    // Remove the real DeviceSettingsProvider and replace it with a stub that
    // allows modifications in a test.
    device_settings_provider_ = cros_settings_->GetProvider(
        chromeos::kReportDeviceVersionInfo);
    EXPECT_TRUE(device_settings_provider_ != NULL);
    EXPECT_TRUE(cros_settings_->RemoveSettingsProvider(
        device_settings_provider_));
    cros_settings_->AddSettingsProvider(&stub_settings_provider_);
  }

  virtual void TearDown() OVERRIDE {
    // Restore the real DeviceSettingsProvider.
    EXPECT_TRUE(
        cros_settings_->RemoveSettingsProvider(&stub_settings_provider_));
    cros_settings_->AddSettingsProvider(device_settings_provider_);
  }

  void SetVariationsRestrictParameterPolicyValue(std::string value) {
    cros_settings_->SetString(chromeos::kVariationsRestrictParameter, value);
  }

 private:
  chromeos::CrosSettings* cros_settings_;
  chromeos::StubCrosSettingsProvider stub_settings_provider_;
  chromeos::CrosSettingsProvider* device_settings_provider_;

  DISALLOW_COPY_AND_ASSIGN(VariationsServiceTestChromeOS);
};

TEST_F(VariationsServiceTestChromeOS, VariationsURLIsValid) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  const std::string default_variations_url =
      VariationsService::GetDefaultVariationsServerURLForTesting();

  std::string value;
  GURL url = VariationsService::GetVariationsServerURL(&prefs);
  EXPECT_TRUE(StartsWithASCII(url.spec(), default_variations_url, true));
  EXPECT_FALSE(net::GetValueForKeyInQuery(url, "restrict", &value));

  SetVariationsRestrictParameterPolicyValue("restricted");
  url = VariationsService::GetVariationsServerURL(&prefs);
  EXPECT_TRUE(StartsWithASCII(url.spec(), default_variations_url, true));
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("restricted", value);
}
#endif

TEST_F(VariationsServiceTest, VariationsURLHasOSNameParam) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  const GURL url = VariationsService::GetVariationsServerURL(&prefs);

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "osname", &value));
  EXPECT_FALSE(value.empty());
}

TEST_F(VariationsServiceTest, LoadSeed) {
  // Store good seed data to test if loading from prefs works.
  const VariationsSeed seed = CreateTestSeed();
  std::string seed_hash;
  const std::string base64_seed = SerializeSeedBase64(seed, &seed_hash);

  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSeed, base64_seed);

  TestVariationsService variations_service(new TestRequestAllowedNotifier,
                                           &prefs);
  VariationsSeed loaded_seed;
  // Check that loading a seed without a hash pref set works correctly.
  EXPECT_TRUE(variations_service.LoadVariationsSeedFromPref(&loaded_seed));

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  // Make sure the pref hasn't been changed.
  EXPECT_FALSE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  EXPECT_EQ(base64_seed, prefs.GetString(prefs::kVariationsSeed));

  // Check that loading a seed with the correct hash works.
  prefs.SetString(prefs::kVariationsSeedHash, seed_hash);
  loaded_seed.Clear();
  EXPECT_TRUE(variations_service.LoadVariationsSeedFromPref(&loaded_seed));
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));

  // Check that false is returned and the pref is cleared when hash differs.
  VariationsSeed different_seed = seed;
  different_seed.mutable_study(0)->set_name("octopus");
  std::string different_hash;
  prefs.SetString(prefs::kVariationsSeed,
                  SerializeSeedBase64(different_seed, &different_hash));
  ASSERT_NE(different_hash, prefs.GetString(prefs::kVariationsSeedHash));
  EXPECT_FALSE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  EXPECT_FALSE(variations_service.LoadVariationsSeedFromPref(&loaded_seed));
  EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  EXPECT_TRUE(
      prefs.FindPreference(prefs::kVariationsSeedDate)->IsDefaultValue());
  EXPECT_TRUE(
      prefs.FindPreference(prefs::kVariationsSeedHash)->IsDefaultValue());

  // Check that loading a bad seed returns false and clears the pref.
  prefs.ClearPref(prefs::kVariationsSeed);
  prefs.SetString(prefs::kVariationsSeed, "this should fail");
  EXPECT_FALSE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  EXPECT_FALSE(variations_service.LoadVariationsSeedFromPref(&loaded_seed));
  EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  EXPECT_TRUE(
      prefs.FindPreference(prefs::kVariationsSeedDate)->IsDefaultValue());
  EXPECT_TRUE(
      prefs.FindPreference(prefs::kVariationsSeedHash)->IsDefaultValue());

  // Check that having no seed in prefs results in a return value of false.
  prefs.ClearPref(prefs::kVariationsSeed);
  EXPECT_FALSE(variations_service.LoadVariationsSeedFromPref(&loaded_seed));
}

TEST_F(VariationsServiceTest, StoreSeed) {
  const base::Time now = base::Time::Now();
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);

  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  TestVariationsService variations_service(new TestRequestAllowedNotifier,
                                           &prefs);

  EXPECT_TRUE(variations_service.StoreSeedData(serialized_seed, now));
  // Make sure the pref was actually set.
  EXPECT_FALSE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());

  std::string loaded_serialized_seed = prefs.GetString(prefs::kVariationsSeed);
  std::string decoded_serialized_seed;
  ASSERT_TRUE(base::Base64Decode(loaded_serialized_seed,
                                 &decoded_serialized_seed));
  // Make sure the stored seed from pref is the same as the seed we created.
  EXPECT_EQ(serialized_seed, decoded_serialized_seed);

  // Check if trying to store a bad seed leaves the pref unchanged.
  prefs.ClearPref(prefs::kVariationsSeed);
  EXPECT_FALSE(variations_service.StoreSeedData("should fail", now));
  EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
}

TEST_F(VariationsServiceTest, RequestsInitiallyNotAllowed) {
  base::MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  // Pass ownership to TestVariationsService, but keep a weak pointer to
  // manipulate it for this test.
  TestRequestAllowedNotifier* test_notifier = new TestRequestAllowedNotifier;
  TestVariationsService test_service(test_notifier, &prefs);

  // Force the notifier to initially disallow requests.
  test_notifier->SetRequestsAllowedOverride(false);
  test_service.StartRepeatedVariationsSeedFetch();
  EXPECT_FALSE(test_service.fetch_attempted());

  test_notifier->NotifyObserver();
  EXPECT_TRUE(test_service.fetch_attempted());
}

TEST_F(VariationsServiceTest, RequestsInitiallyAllowed) {
  base::MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  // Pass ownership to TestVariationsService, but keep a weak pointer to
  // manipulate it for this test.
  TestRequestAllowedNotifier* test_notifier = new TestRequestAllowedNotifier;
  TestVariationsService test_service(test_notifier, &prefs);

  test_notifier->SetRequestsAllowedOverride(true);
  test_service.StartRepeatedVariationsSeedFetch();
  EXPECT_TRUE(test_service.fetch_attempted());
}

TEST_F(VariationsServiceTest, SeedStoredWhenOKStatus) {
  base::MessageLoop message_loop;
  content::TestBrowserThread io_thread(content::BrowserThread::IO,
                                       &message_loop);
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  VariationsService variations_service(new TestRequestAllowedNotifier, &prefs);

  net::TestURLFetcherFactory factory;
  variations_service.DoActualFetch();

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  SimulateServerResponse(net::HTTP_OK, fetcher);
  const VariationsSeed seed = CreateTestSeed();
  fetcher->SetResponseString(SerializeSeed(seed));

  EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  variations_service.OnURLFetchComplete(fetcher);
  EXPECT_FALSE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  const std::string expected_base64 = SerializeSeedBase64(seed, NULL);
  EXPECT_EQ(expected_base64, prefs.GetString(prefs::kVariationsSeed));
}

TEST_F(VariationsServiceTest, SeedNotStoredWhenNonOKStatus) {
  const int non_ok_status_codes[] = {
    net::HTTP_NO_CONTENT,
    net::HTTP_NOT_MODIFIED,
    net::HTTP_NOT_FOUND,
    net::HTTP_INTERNAL_SERVER_ERROR,
    net::HTTP_SERVICE_UNAVAILABLE,
  };

  base::MessageLoop message_loop;
  content::TestBrowserThread io_thread(content::BrowserThread::IO,
                                       &message_loop);
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  VariationsService variations_service(new TestRequestAllowedNotifier, &prefs);
  for (size_t i = 0; i < arraysize(non_ok_status_codes); ++i) {
    net::TestURLFetcherFactory factory;
    variations_service.DoActualFetch();
    EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());

    net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
    SimulateServerResponse(non_ok_status_codes[i], fetcher);
    variations_service.OnURLFetchComplete(fetcher);

    EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  }
}

TEST_F(VariationsServiceTest, SeedDateUpdatedOn304Status) {
  base::MessageLoop message_loop;
  content::TestBrowserThread io_thread(content::BrowserThread::IO,
                                       &message_loop);
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  VariationsService variations_service(new TestRequestAllowedNotifier, &prefs);
  net::TestURLFetcherFactory factory;
  variations_service.DoActualFetch();
  EXPECT_TRUE(
      prefs.FindPreference(prefs::kVariationsSeedDate)->IsDefaultValue());

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  SimulateServerResponse(net::HTTP_NOT_MODIFIED, fetcher);
  variations_service.OnURLFetchComplete(fetcher);
  EXPECT_FALSE(
      prefs.FindPreference(prefs::kVariationsSeedDate)->IsDefaultValue());
}

}  // namespace chrome_variations
