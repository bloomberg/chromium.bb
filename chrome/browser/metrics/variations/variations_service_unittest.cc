// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base64.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_split.h"
#include "chrome/browser/metrics/proto/study.pb.h"
#include "chrome/browser/metrics/variations/resource_request_allowed_notifier_test_util.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_variations {

namespace {

// A test class used to validate expected functionality in VariationsService.
class TestVariationsService : public VariationsService {
 public:
  explicit TestVariationsService(TestRequestAllowedNotifier* test_notifier)
      : VariationsService(test_notifier),
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

// Converts |time| to Study proto format.
int64 TimeToProtoTime(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InSeconds();
}

// Populates |seed| with simple test data. The resulting seed will contain one
// study called "test", which contains one experiment called "abc" with
// probability weight 100. |seed|'s study field will be cleared before adding
// the new study.
TrialsSeed CreateTestSeed() {
  TrialsSeed seed;
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
std::string SerializeSeed(const TrialsSeed& seed) {
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return serialized_seed;
}

// Serializes |seed| to base64-encoded protobuf binary format.
std::string SerializeSeedBase64(const TrialsSeed& seed) {
  std::string serialized_seed = SerializeSeed(seed);
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

TEST(VariationsServiceTest, CheckStudyChannel) {
  const chrome::VersionInfo::Channel channels[] = {
    chrome::VersionInfo::CHANNEL_CANARY,
    chrome::VersionInfo::CHANNEL_DEV,
    chrome::VersionInfo::CHANNEL_BETA,
    chrome::VersionInfo::CHANNEL_STABLE,
  };
  const Study_Channel study_channels[] = {
    Study_Channel_CANARY,
    Study_Channel_DEV,
    Study_Channel_BETA,
    Study_Channel_STABLE,
  };
  ASSERT_EQ(arraysize(channels), arraysize(study_channels));
  bool channel_added[arraysize(channels)] = { 0 };

  Study_Filter filter;

  // Check in the forwarded order. The loop cond is <= arraysize(study_channels)
  // instead of < so that the result of adding the last channel gets checked.
  for (size_t i = 0; i <= arraysize(study_channels); ++i) {
    for (size_t j = 0; j < arraysize(channels); ++j) {
      const bool expected = channel_added[j] || filter.channel_size() == 0;
      const bool result = VariationsService::CheckStudyChannel(filter,
                                                               channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(study_channels)) {
      filter.add_channel(study_channels[i]);
      channel_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_channel();
  memset(&channel_added, 0, sizeof(channel_added));
  for (size_t i = 0; i <= arraysize(study_channels); ++i) {
    for (size_t j = 0; j < arraysize(channels); ++j) {
      const bool expected = channel_added[j] || filter.channel_size() == 0;
      const bool result = VariationsService::CheckStudyChannel(filter,
                                                               channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(study_channels)) {
      const int index = arraysize(study_channels) - i - 1;
      filter.add_channel(study_channels[index]);
      channel_added[index] = true;
    }
  }
}

TEST(VariationsServiceTest, CheckStudyLocale) {
  struct {
    const char* filter_locales;
    bool en_us_result;
    bool en_ca_result;
    bool fr_result;
  } test_cases[] = {
    {"en-US", true, false, false},
    {"en-US,en-CA,fr", true, true, true},
    {"en-US,en-CA,en-GB", true, true, false},
    {"en-GB,en-CA,en-US", true, true, false},
    {"ja,kr,vi", false, false, false},
    {"fr-CA", false, false, false},
    {"", true, true, true},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    std::vector<std::string> filter_locales;
    Study_Filter filter;
    base::SplitString(test_cases[i].filter_locales, ',', &filter_locales);
    for (size_t j = 0; j < filter_locales.size(); ++j)
      filter.add_locale(filter_locales[j]);
    EXPECT_EQ(test_cases[i].en_us_result,
              VariationsService::CheckStudyLocale(filter, "en-US"));
    EXPECT_EQ(test_cases[i].en_ca_result,
              VariationsService::CheckStudyLocale(filter, "en-CA"));
    EXPECT_EQ(test_cases[i].fr_result,
              VariationsService::CheckStudyLocale(filter, "fr"));
  }
}

TEST(VariationsServiceTest, CheckStudyPlatform) {
  const Study_Platform platforms[] = {
    Study_Platform_PLATFORM_WINDOWS,
    Study_Platform_PLATFORM_MAC,
    Study_Platform_PLATFORM_LINUX,
    Study_Platform_PLATFORM_CHROMEOS,
    Study_Platform_PLATFORM_ANDROID,
    Study_Platform_PLATFORM_IOS,
  };
  ASSERT_EQ(Study_Platform_Platform_ARRAYSIZE,
            static_cast<int>(arraysize(platforms)));
  bool platform_added[arraysize(platforms)] = { 0 };

  Study_Filter filter;

  // Check in the forwarded order. The loop cond is <= arraysize(platforms)
  // instead of < so that the result of adding the last channel gets checked.
  for (size_t i = 0; i <= arraysize(platforms); ++i) {
    for (size_t j = 0; j < arraysize(platforms); ++j) {
      const bool expected = platform_added[j] || filter.platform_size() == 0;
      const bool result = VariationsService::CheckStudyPlatform(filter,
                                                                platforms[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(platforms)) {
      filter.add_platform(platforms[i]);
      platform_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_platform();
  memset(&platform_added, 0, sizeof(platform_added));
  for (size_t i = 0; i <= arraysize(platforms); ++i) {
    for (size_t j = 0; j < arraysize(platforms); ++j) {
      const bool expected = platform_added[j] || filter.platform_size() == 0;
      const bool result = VariationsService::CheckStudyPlatform(filter,
                                                                platforms[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(platforms)) {
      const int index = arraysize(platforms) - i - 1;
      filter.add_platform(platforms[index]);
      platform_added[index] = true;
    }
  }
}

TEST(VariationsServiceTest, CheckStudyVersion) {
  const struct {
    const char* min_version;
    const char* version;
    bool expected_result;
  } min_test_cases[] = {
    { "1.2.2", "1.2.3", true },
    { "1.2.3", "1.2.3", true },
    { "1.2.4", "1.2.3", false },
    { "1.3.2", "1.2.3", false },
    { "2.1.2", "1.2.3", false },
    { "0.3.4", "1.2.3", true },
    // Wildcards.
    { "1.*", "1.2.3", true },
    { "1.2.*", "1.2.3", true },
    { "1.2.3.*", "1.2.3", true },
    { "1.2.4.*", "1.2.3", false },
    { "2.*", "1.2.3", false },
    { "0.3.*", "1.2.3", true },
  };

  const struct {
    const char* max_version;
    const char* version;
    bool expected_result;
  } max_test_cases[] = {
    { "1.2.2", "1.2.3", false },
    { "1.2.3", "1.2.3", true },
    { "1.2.4", "1.2.3", true },
    { "2.1.1", "1.2.3", true },
    { "2.1.1", "2.3.4", false },
    // Wildcards
    { "2.1.*", "2.3.4", false },
    { "2.*", "2.3.4", true },
    { "2.3.*", "2.3.4", true },
    { "2.3.4.*", "2.3.4", true },
    { "2.3.4.0.*", "2.3.4", true },
    { "2.4.*", "2.3.4", true },
    { "1.3.*", "2.3.4", false },
    { "1.*", "2.3.4", false },
  };

  Study_Filter filter;

  // Min/max version not set should result in true.
  EXPECT_TRUE(VariationsService::CheckStudyVersion(filter, "1.2.3"));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(min_test_cases); ++i) {
    filter.set_min_version(min_test_cases[i].min_version);
    const bool result =
        VariationsService::CheckStudyVersion(filter, min_test_cases[i].version);
    EXPECT_EQ(min_test_cases[i].expected_result, result) <<
        "Min. version case " << i << " failed!";
  }
  filter.clear_min_version();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(max_test_cases); ++i) {
    filter.set_max_version(max_test_cases[i].max_version);
    const bool result =
        VariationsService::CheckStudyVersion(filter, max_test_cases[i].version);
    EXPECT_EQ(max_test_cases[i].expected_result, result) <<
        "Max version case " << i << " failed!";
  }

  // Check intersection semantics.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(min_test_cases); ++i) {
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(max_test_cases); ++j) {
      filter.set_min_version(min_test_cases[i].min_version);
      filter.set_max_version(max_test_cases[j].max_version);

      if (!min_test_cases[i].expected_result) {
        const bool result =
            VariationsService::CheckStudyVersion(filter,
                                                 min_test_cases[i].version);
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }

      if (!max_test_cases[j].expected_result) {
        const bool result =
            VariationsService::CheckStudyVersion(filter,
                                                 max_test_cases[j].version);
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }
    }
  }
}

TEST(VariationsServiceTest, CheckStudyStartDate) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::TimeDelta::FromHours(1);
  const struct {
    const base::Time start_date;
    bool expected_result;
  } start_test_cases[] = {
    { now - delta, true },
    { now, true },
    { now + delta, false },
  };

  Study_Filter filter;

  // Start date not set should result in true.
  EXPECT_TRUE(VariationsService::CheckStudyStartDate(filter, now));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(start_test_cases); ++i) {
    filter.set_start_date(TimeToProtoTime(start_test_cases[i].start_date));
    const bool result = VariationsService::CheckStudyStartDate(filter, now);
    EXPECT_EQ(start_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsServiceTest, IsStudyExpired) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::TimeDelta::FromHours(1);
  const struct {
    const base::Time expiry_date;
    bool expected_result;
  } expiry_test_cases[] = {
    { now - delta, true },
    { now, true },
    { now + delta, false },
  };

  Study study;

  // Expiry date not set should result in false.
  EXPECT_FALSE(VariationsService::IsStudyExpired(study, now));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expiry_test_cases); ++i) {
    study.set_expiry_date(TimeToProtoTime(expiry_test_cases[i].expiry_date));
    const bool result = VariationsService::IsStudyExpired(study, now);
    EXPECT_EQ(expiry_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsServiceTest, VariationsURLIsValid) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  const std::string default_variations_url =
      VariationsService::GetDefaultVariationsServerURLForTesting();

  EXPECT_EQ(default_variations_url,
            VariationsService::GetVariationsServerURL(&prefs).spec());

  prefs.SetString(prefs::kVariationsRestrictParameter, "restricted");
  EXPECT_EQ(default_variations_url + "?restrict=restricted",
            VariationsService::GetVariationsServerURL(&prefs).spec());
}

TEST(VariationsServiceTest, LoadSeed) {
  // Store good seed data to test if loading from prefs works.
  const TrialsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);

  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSeed, base64_seed);

  TestVariationsService variations_service(new TestRequestAllowedNotifier);
  TrialsSeed loaded_seed;
  EXPECT_TRUE(variations_service.LoadTrialsSeedFromPref(&prefs, &loaded_seed));

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  // Make sure the pref hasn't been changed.
  EXPECT_FALSE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  EXPECT_EQ(base64_seed, prefs.GetString(prefs::kVariationsSeed));

  // Check that loading a bad seed returns false and clears the pref.
  prefs.ClearPref(prefs::kVariationsSeed);
  prefs.SetString(prefs::kVariationsSeed, "this should fail");
  EXPECT_FALSE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  EXPECT_FALSE(variations_service.LoadTrialsSeedFromPref(&prefs, &loaded_seed));
  EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());

  // Check that having no seed in prefs results in a return value of false.
  prefs.ClearPref(prefs::kVariationsSeed);
  EXPECT_FALSE(variations_service.LoadTrialsSeedFromPref(&prefs, &loaded_seed));
}

TEST(VariationsServiceTest, StoreSeed) {
  const base::Time now = base::Time::Now();
  const TrialsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);

  TestVariationsService variations_service(new TestRequestAllowedNotifier);

  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  EXPECT_TRUE(variations_service.StoreSeedData(serialized_seed, now, &prefs));
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
  EXPECT_FALSE(variations_service.StoreSeedData("should fail", now, &prefs));
  EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
}

TEST(VariationsServiceTest, ValidateStudy) {
  Study study;
  study.set_default_experiment_name("def");

  Study_Experiment* experiment = study.add_experiment();
  experiment->set_name("abc");
  experiment->set_probability_weight(100);

  Study_Experiment* default_group = study.add_experiment();
  default_group->set_name("def");
  default_group->set_probability_weight(200);

  base::FieldTrial::Probability total_probability = 0;
  bool valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);
  EXPECT_EQ(300, total_probability);

  // Min version checks.
  study.mutable_filter()->set_min_version("1.2.3.*");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);
  study.mutable_filter()->set_min_version("1.*.3");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_FALSE(valid);
  study.mutable_filter()->set_min_version("1.2.3");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);

  // Max version checks.
  study.mutable_filter()->set_max_version("2.3.4.*");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);
  study.mutable_filter()->set_max_version("*.3");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_FALSE(valid);
  study.mutable_filter()->set_max_version("2.3.4");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);

  study.clear_default_experiment_name();
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);

  study.set_default_experiment_name("xyz");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);

  study.set_default_experiment_name("def");
  default_group->clear_name();
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);

  default_group->set_name("def");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  ASSERT_TRUE(valid);
  Study_Experiment* repeated_group = study.add_experiment();
  repeated_group->set_name("abc");
  repeated_group->set_probability_weight(1);
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);
}

TEST(VariationsServiceTest, RequestsInitiallyNotAllowed) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);

  // Pass ownership to TestVariationsService, but keep a weak pointer to
  // manipulate it for this test.
  TestRequestAllowedNotifier* test_notifier = new TestRequestAllowedNotifier;
  TestVariationsService test_service(test_notifier);

  // Force the notifier to initially disallow requests.
  test_notifier->SetRequestsAllowedOverride(false);
  test_service.StartRepeatedVariationsSeedFetch();
  EXPECT_FALSE(test_service.fetch_attempted());

  test_notifier->NotifyObserver();
  EXPECT_TRUE(test_service.fetch_attempted());
}

TEST(VariationsServiceTest, RequestsInitiallyAllowed) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);

  // Pass ownership to TestVariationsService, but keep a weak pointer to
  // manipulate it for this test.
  TestRequestAllowedNotifier* test_notifier = new TestRequestAllowedNotifier;
  TestVariationsService test_service(test_notifier);

  test_notifier->SetRequestsAllowedOverride(true);
  test_service.StartRepeatedVariationsSeedFetch();
  EXPECT_TRUE(test_service.fetch_attempted());
}

TEST(VariationsServiceTest, SeedStoredWhenOKStatus) {
  MessageLoop message_loop;
  content::TestBrowserThread io_thread(content::BrowserThread::IO,
                                       &message_loop);
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  VariationsService variations_service(&prefs);

  net::TestURLFetcherFactory factory;
  variations_service.DoActualFetch();

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  SimulateServerResponse(net::HTTP_OK, fetcher);
  const TrialsSeed seed = CreateTestSeed();
  fetcher->SetResponseString(SerializeSeed(seed));

  EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  variations_service.OnURLFetchComplete(fetcher);
  EXPECT_FALSE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  EXPECT_EQ(SerializeSeedBase64(seed), prefs.GetString(prefs::kVariationsSeed));
}

TEST(VariationsServiceTest, SeedNotStoredWhenNonOKStatus) {
  const int non_ok_status_codes[] = {
    net::HTTP_NO_CONTENT,
    net::HTTP_NOT_MODIFIED,
    net::HTTP_NOT_FOUND,
    net::HTTP_INTERNAL_SERVER_ERROR,
    net::HTTP_SERVICE_UNAVAILABLE,
  };

  MessageLoop message_loop;
  content::TestBrowserThread io_thread(content::BrowserThread::IO,
                                       &message_loop);
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  VariationsService variations_service(&prefs);
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

}  // namespace chrome_variations
