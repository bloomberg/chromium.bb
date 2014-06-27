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
#include "chrome/test/base/testing_pref_service_syncable.h"
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
      : VariationsService(test_notifier, local_state, NULL),
        intercepts_fetch_(true),
        fetch_attempted_(false),
        seed_stored_(false) {
    // Set this so StartRepeatedVariationsSeedFetch can be called in tests.
    SetCreateTrialsFromSeedCalledForTesting(true);
  }

  virtual ~TestVariationsService() {
  }

  void set_intercepts_fetch(bool value) {
    intercepts_fetch_ = value;
  }

  bool fetch_attempted() const { return fetch_attempted_; }

  bool seed_stored() const { return seed_stored_; }

  virtual void DoActualFetch() OVERRIDE {
    if (intercepts_fetch_) {
      fetch_attempted_ = true;
      return;
    }

    VariationsService::DoActualFetch();
  }

 protected:
  virtual void StoreSeed(const std::string& seed_data,
                         const std::string& seed_signature,
                         const base::Time& date_fetched) OVERRIDE {
    seed_stored_ = true;
  }

 private:
  bool intercepts_fetch_;
  bool fetch_attempted_;
  bool seed_stored_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsService);
};

class TestVariationsServiceObserver : public VariationsService::Observer {
 public:
  TestVariationsServiceObserver()
      : best_effort_changes_notified_(0),
        crticial_changes_notified_(0) {
  }
  virtual ~TestVariationsServiceObserver() {
  }

  virtual void OnExperimentChangesDetected(Severity severity) OVERRIDE {
    switch (severity) {
      case BEST_EFFORT:
        ++best_effort_changes_notified_;
        break;
      case CRITICAL:
        ++crticial_changes_notified_;
        break;
    }
  }

  int best_effort_changes_notified() const {
    return best_effort_changes_notified_;
  }

  int crticial_changes_notified() const {
    return crticial_changes_notified_;
  }

 private:
  // Number of notification received with BEST_EFFORT severity.
  int best_effort_changes_notified_;

  // Number of notification received with CRITICAL severity.
  int crticial_changes_notified_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsServiceObserver);
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
#if defined(OS_ANDROID)
  // Android uses profile prefs as the PrefService to generate the URL.
  TestingPrefServiceSyncable prefs;
  VariationsService::RegisterProfilePrefs(prefs.registry());
#else
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
#endif
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

  TestVariationsService service(new TestRequestAllowedNotifier, &prefs);
  service.set_intercepts_fetch(false);

  net::TestURLFetcherFactory factory;
  service.DoActualFetch();

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  SimulateServerResponse(net::HTTP_OK, fetcher);
  fetcher->SetResponseString(SerializeSeed(CreateTestSeed()));

  EXPECT_FALSE(service.seed_stored());
  service.OnURLFetchComplete(fetcher);
  EXPECT_TRUE(service.seed_stored());
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

  VariationsService service(new TestRequestAllowedNotifier, &prefs, NULL);
  for (size_t i = 0; i < arraysize(non_ok_status_codes); ++i) {
    net::TestURLFetcherFactory factory;
    service.DoActualFetch();
    EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());

    net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
    SimulateServerResponse(non_ok_status_codes[i], fetcher);
    service.OnURLFetchComplete(fetcher);

    EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  }
}

TEST_F(VariationsServiceTest, SeedDateUpdatedOn304Status) {
  base::MessageLoop message_loop;
  content::TestBrowserThread io_thread(content::BrowserThread::IO,
                                       &message_loop);
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  VariationsService service(new TestRequestAllowedNotifier, &prefs, NULL);
  net::TestURLFetcherFactory factory;
  service.DoActualFetch();
  EXPECT_TRUE(
      prefs.FindPreference(prefs::kVariationsSeedDate)->IsDefaultValue());

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  SimulateServerResponse(net::HTTP_NOT_MODIFIED, fetcher);
  service.OnURLFetchComplete(fetcher);
  EXPECT_FALSE(
      prefs.FindPreference(prefs::kVariationsSeedDate)->IsDefaultValue());
}

TEST_F(VariationsServiceTest, Observer) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  VariationsService service(new TestRequestAllowedNotifier, &prefs, NULL);

  struct {
    int normal_count;
    int best_effort_count;
    int critical_count;
    int expected_best_effort_notifications;
    int expected_crtical_notifications;
  } cases[] = {
      {0, 0, 0, 0, 0},
      {1, 0, 0, 0, 0},
      {10, 0, 0, 0, 0},
      {0, 1, 0, 1, 0},
      {0, 10, 0, 1, 0},
      {0, 0, 1, 0, 1},
      {0, 0, 10, 0, 1},
      {0, 1, 1, 0, 1},
      {1, 1, 1, 0, 1},
      {1, 1, 0, 1, 0},
      {1, 0, 1, 0, 1},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    TestVariationsServiceObserver observer;
    service.AddObserver(&observer);

    VariationsSeedSimulator::Result result;
    result.normal_group_change_count = cases[i].normal_count;
    result.kill_best_effort_group_change_count = cases[i].best_effort_count;
    result.kill_critical_group_change_count = cases[i].critical_count;
    service.NotifyObservers(result);

    EXPECT_EQ(cases[i].expected_best_effort_notifications,
              observer.best_effort_changes_notified()) << i;
    EXPECT_EQ(cases[i].expected_crtical_notifications,
              observer.crticial_changes_notified()) << i;

    service.RemoveObserver(&observer);
  }
}

}  // namespace chrome_variations
