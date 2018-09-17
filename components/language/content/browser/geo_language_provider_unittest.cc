// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/geo_language_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "components/language/content/browser/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {

class GeoLanguageProviderTest : public testing::Test {
 public:
  GeoLanguageProviderTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
            base::TestMockTimeTaskRunner::Type::kBoundToThread)),
        scoped_context_(task_runner_.get()),
        geo_language_provider_(task_runner_),
        mock_ip_geo_location_provider_(&mock_geo_location_) {
    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);
    service_manager::Connector::TestApi test_api(connector_.get());
    test_api.OverrideBinderForTesting(
        service_manager::Identity(device::mojom::kServiceName),
        device::mojom::PublicIpAddressGeolocationProvider::Name_,
        base::BindRepeating(&MockIpGeoLocationProvider::Bind,
                            base::Unretained(&mock_ip_geo_location_provider_)));
    language::GeoLanguageProvider::RegisterLocalStatePrefs(
        local_state_.registry());
  }

 protected:
  std::vector<std::string> GetCurrentGeoLanguages() {
    return geo_language_provider_.CurrentGeoLanguages();
  }

  void StartGeoLanguageProvider() {
    geo_language_provider_.StartUp(std::move(connector_), &local_state_);
  }

  void MoveToLocation(float latitude, float longitude) {
    mock_geo_location_.MoveToLocation(latitude, longitude);
  }

  const scoped_refptr<base::TestMockTimeTaskRunner>& GetTaskRunner() {
    return task_runner_;
  }

  int GetQueryNextPositionCalledTimes() {
    return mock_geo_location_.query_next_position_called_times();
  }

  void SetGeoLanguages(const std::vector<std::string>& languages) {
    geo_language_provider_.SetGeoLanguages(languages);
  }

  void SetUpCachedLanguages(const std::vector<std::string>& languages) {
    base::ListValue cache_list;
    for (size_t i = 0; i < languages.size(); ++i) {
      cache_list.Set(i, std::make_unique<base::Value>(languages[i]));
    }
    local_state_.Set(GeoLanguageProvider::kCachedGeoLanguagesPref, cache_list);
  }

  const std::vector<std::string> GetCachedLanguages() {
    std::vector<std::string> languages;
    const base::ListValue* const cached_languages_list =
        local_state_.GetList(GeoLanguageProvider::kCachedGeoLanguagesPref);
    for (const auto& language_value : *cached_languages_list) {
      languages.push_back(language_value.GetString());
    }
    return languages;
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  const base::TestMockTimeTaskRunner::ScopedContext scoped_context_;

  // Object under test.
  GeoLanguageProvider geo_language_provider_;
  MockGeoLocation mock_geo_location_;
  MockIpGeoLocationProvider mock_ip_geo_location_provider_;
  std::unique_ptr<service_manager::Connector> connector_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(GeoLanguageProviderTest, GetCurrentGeoLanguages) {
  // Setup a random place in Madhya Pradesh, India.
  MoveToLocation(23.0, 80.0);
  StartGeoLanguageProvider();
  const auto task_runner = GetTaskRunner();
  task_runner->RunUntilIdle();

  const std::vector<std::string>& result = GetCurrentGeoLanguages();
  std::vector<std::string> expected_langs = {"hi", "mr", "ur"};
  EXPECT_EQ(expected_langs, result);
  EXPECT_EQ(1, GetQueryNextPositionCalledTimes());
  EXPECT_EQ(expected_langs, GetCachedLanguages());
}

TEST_F(GeoLanguageProviderTest, NoFrequentCalls) {
  // Setup a random place in Madhya Pradesh, India.
  MoveToLocation(23.0, 80.0);
  StartGeoLanguageProvider();
  const auto task_runner = GetTaskRunner();
  task_runner->RunUntilIdle();

  const std::vector<std::string>& result = GetCurrentGeoLanguages();
  std::vector<std::string> expected_langs = {"hi", "mr", "ur"};
  EXPECT_EQ(expected_langs, result);

  task_runner->FastForwardBy(base::TimeDelta::FromHours(12));
  EXPECT_EQ(1, GetQueryNextPositionCalledTimes());
  EXPECT_EQ(expected_langs, GetCachedLanguages());
}

TEST_F(GeoLanguageProviderTest, ButDoCallInTheNextDay) {
  // Setup a random place in Madhya Pradesh, India.
  MoveToLocation(23.0, 80.0);
  StartGeoLanguageProvider();
  const auto task_runner = GetTaskRunner();
  task_runner->RunUntilIdle();

  std::vector<std::string> result = GetCurrentGeoLanguages();
  std::vector<std::string> expected_langs = {"hi", "mr", "ur"};
  EXPECT_EQ(expected_langs, result);
  EXPECT_EQ(expected_langs, GetCachedLanguages());

  // Move to another random place in Karnataka, India.
  MoveToLocation(15.0, 75.0);
  task_runner->FastForwardBy(base::TimeDelta::FromHours(25));
  EXPECT_EQ(2, GetQueryNextPositionCalledTimes());

  result = GetCurrentGeoLanguages();
  std::vector<std::string> expected_langs_2 = {"kn", "ur", "te", "mr", "ta"};
  EXPECT_EQ(expected_langs_2, result);
  EXPECT_EQ(expected_langs_2, GetCachedLanguages());
}

TEST_F(GeoLanguageProviderTest, CachedLanguagesPresent) {
  SetUpCachedLanguages({"en", "fr"});
  MoveToLocation(23.0, 80.0);
  StartGeoLanguageProvider();

  std::vector<std::string> expected_langs = {"en", "fr"};
  EXPECT_EQ(expected_langs, GetCurrentGeoLanguages());

  const auto task_runner = GetTaskRunner();
  task_runner->RunUntilIdle();

  expected_langs = {"hi", "mr", "ur"};
  EXPECT_EQ(expected_langs, GetCurrentGeoLanguages());
  EXPECT_EQ(expected_langs, GetCachedLanguages());
}

}  // namespace language
