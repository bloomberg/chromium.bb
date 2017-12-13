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
#include "device/geolocation/public/interfaces/geolocation.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/device/public/interfaces/public_ip_address_geolocation_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Mock impl of mojom::Geolocation that allows tests to control the returned
// location.
class MockGeoLocation : public device::mojom::Geolocation {
 public:
  MockGeoLocation() : binding_(this) {}
  // device::mojom::Geolocation implementation:
  void SetHighAccuracy(bool high_accuracy) override {}
  void QueryNextPosition(QueryNextPositionCallback callback) override {
    ++query_next_position_called_times_;
    std::move(callback).Run(position_.Clone());
  }

  void BindGeoLocation(device::mojom::GeolocationRequest request) {
    binding_.Bind(std::move(request));
  }

  void MoveToLocation(float latitude, float longitude) {
    position_.latitude = latitude;
    position_.longitude = longitude;
  }

  int query_next_position_called_times() const {
    return query_next_position_called_times_;
  }

 private:
  int query_next_position_called_times_ = 0;
  device::mojom::Geoposition position_;
  mojo::Binding<device::mojom::Geolocation> binding_;
};

// Mock impl of mojom::PublicIpAddressGeolocationProvider that binds Geolocation
// to testing impl.
class MockIpGeoLocationProvider
    : public device::mojom::PublicIpAddressGeolocationProvider {
 public:
  explicit MockIpGeoLocationProvider(MockGeoLocation* mock_geo_location)
      : mock_geo_location_(mock_geo_location), binding_(this) {}

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    binding_.Bind(device::mojom::PublicIpAddressGeolocationProviderRequest(
        std::move(handle)));
  }

  void CreateGeolocation(
      const net::MutablePartialNetworkTrafficAnnotationTag& /* unused */,
      device::mojom::GeolocationRequest request) override {
    mock_geo_location_->BindGeoLocation(std::move(request));
  }

 private:
  MockGeoLocation* mock_geo_location_;

  mojo::Binding<device::mojom::PublicIpAddressGeolocationProvider> binding_;
};

}  // namespace

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
        device::mojom::kServiceName,
        device::mojom::PublicIpAddressGeolocationProvider::Name_,
        base::BindRepeating(&MockIpGeoLocationProvider::Bind,
                            base::Unretained(&mock_ip_geo_location_provider_)));
  }

 protected:
  std::vector<std::string> GetCurrentGeoLanguages() {
    return geo_language_provider_.CurrentGeoLanguages();
  }

  void StartGeoLanguageProvider() {
    geo_language_provider_.StartUp(std::move(connector_));
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

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  const base::TestMockTimeTaskRunner::ScopedContext scoped_context_;

  // Object under test.
  GeoLanguageProvider geo_language_provider_;
  MockGeoLocation mock_geo_location_;
  MockIpGeoLocationProvider mock_ip_geo_location_provider_;
  std::unique_ptr<service_manager::Connector> connector_;
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

  // Move to another random place in Karnataka, India.
  MoveToLocation(15.0, 75.0);
  task_runner->FastForwardBy(base::TimeDelta::FromHours(25));
  EXPECT_EQ(2, GetQueryNextPositionCalledTimes());

  result = GetCurrentGeoLanguages();
  std::vector<std::string> expected_langs_2 = {"kn", "ur", "te", "mr", "ta"};
  EXPECT_EQ(expected_langs_2, result);
}

}  // namespace language
