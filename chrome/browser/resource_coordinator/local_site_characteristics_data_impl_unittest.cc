// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {
namespace internal {

namespace {

constexpr char kDummyOrigin[] = "foo.com";
constexpr base::TimeDelta kInitialTimeSinceEpoch =
    base::TimeDelta::FromSeconds(1);

class TestLocalSiteCharacteristicsDataImpl
    : public LocalSiteCharacteristicsDataImpl {
 public:
  using LocalSiteCharacteristicsDataImpl::
      GetUsesAudioInBackgroundMinObservationWindow;
  using LocalSiteCharacteristicsDataImpl::
      GetUsesNotificationsInBackgroundMinObservationWindow;
  using LocalSiteCharacteristicsDataImpl::FeatureObservationDuration;
  using LocalSiteCharacteristicsDataImpl::site_characteristics_for_testing;
  using LocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation;
  using LocalSiteCharacteristicsDataImpl::last_loaded_time_for_testing;

  explicit TestLocalSiteCharacteristicsDataImpl(const std::string& origin_str)
      : LocalSiteCharacteristicsDataImpl(origin_str) {}

  base::TimeDelta FeatureObservationTimestamp(
      const SiteCharacteristicsFeatureProto& feature_proto) {
    return InternalRepresentationToTimeDelta(feature_proto.use_timestamp());
  }

 protected:
  ~TestLocalSiteCharacteristicsDataImpl() override{};
};

}  // namespace

class LocalSiteCharacteristicsDataImplTest : public testing::Test {
 public:
  LocalSiteCharacteristicsDataImplTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {}

  void SetUp() override {
    test_clock_.SetNowTicks(base::TimeTicks::UnixEpoch());
    // Advance the test clock by a small delay, as some tests will fail if the
    // current time is equal to Epoch.
    test_clock_.Advance(kInitialTimeSinceEpoch);
  }

 protected:
  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
};

TEST_F(LocalSiteCharacteristicsDataImplTest, BasicTestEndToEnd) {
  auto local_site_data =
      base::MakeRefCounted<TestLocalSiteCharacteristicsDataImpl>(kDummyOrigin);

  local_site_data->NotifySiteLoaded();

  // Initially the feature usage should be reported as unknown.
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_USAGE_UNKNOWN,
            local_site_data->UsesAudioInBackground());

  // Advance the clock by a time lower than the miniumum observation time for
  // the audio feature.
  test_clock_.Advance(TestLocalSiteCharacteristicsDataImpl::
                          GetUsesAudioInBackgroundMinObservationWindow() -
                      base::TimeDelta::FromSeconds(1));

  // The audio feature usage is still unknown as the observation window hasn't
  // expired.
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_USAGE_UNKNOWN,
            local_site_data->UsesAudioInBackground());

  // Report that the audio feature has been used.
  local_site_data->NotifyUsesAudioInBackground();
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_IN_USE,
            local_site_data->UsesAudioInBackground());

  // When a feature is in use it's expected that its recorded observation
  // timestamp is equal to the time delta since Unix Epoch when the observation
  // has been made.
  EXPECT_EQ(local_site_data->FeatureObservationTimestamp(
                local_site_data->site_characteristics_for_testing()
                    .uses_audio_in_background()),
            (test_clock_.NowTicks() - base::TimeTicks::UnixEpoch()));
  EXPECT_EQ(local_site_data->FeatureObservationDuration(
                local_site_data->site_characteristics_for_testing()
                    .uses_audio_in_background()),
            base::TimeDelta());

  // Advance the clock and make sure that notifications feature gets
  // reported as unused.
  test_clock_.Advance(
      TestLocalSiteCharacteristicsDataImpl::
          GetUsesNotificationsInBackgroundMinObservationWindow());
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_NOT_IN_USE,
            local_site_data->UsesNotificationsInBackground());

  // Observating that a feature has been used after its observation window has
  // expired should still be recorded, the feature should then be reported as
  // used.
  local_site_data->NotifyUsesNotificationsInBackground();
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_IN_USE,
            local_site_data->UsesNotificationsInBackground());

  local_site_data->NotifySiteUnloaded();
}

TEST_F(LocalSiteCharacteristicsDataImplTest, LastLoadedTime) {
  auto local_site_data =
      base::MakeRefCounted<TestLocalSiteCharacteristicsDataImpl>(kDummyOrigin);
  // Create a second instance of this object, simulates having several tab
  // owning it.
  auto local_site_data2(local_site_data);

  local_site_data->NotifySiteLoaded();
  base::TimeDelta last_loaded_time =
      local_site_data->last_loaded_time_for_testing();

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  // Loading the site a second time shouldn't change the last loaded time.
  local_site_data2->NotifySiteLoaded();
  EXPECT_EQ(last_loaded_time, local_site_data2->last_loaded_time_for_testing());

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  // Unloading the site shouldn't update the last loaded time as there's still
  // a loaded instance.
  local_site_data2->NotifySiteUnloaded();
  EXPECT_EQ(last_loaded_time, local_site_data->last_loaded_time_for_testing());

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  local_site_data->NotifySiteUnloaded();
  EXPECT_NE(last_loaded_time, local_site_data->last_loaded_time_for_testing());
}

TEST_F(LocalSiteCharacteristicsDataImplTest, GetFeatureUsageForUnloadedSite) {
  auto local_site_data =
      base::MakeRefCounted<TestLocalSiteCharacteristicsDataImpl>(kDummyOrigin);

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyUsesAudioInBackground();

  test_clock_.Advance(
      TestLocalSiteCharacteristicsDataImpl::
          GetUsesNotificationsInBackgroundMinObservationWindow() -
      base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_IN_USE,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_USAGE_UNKNOWN,
            local_site_data->UsesNotificationsInBackground());

  const base::TimeDelta observation_duration_before_unload =
      local_site_data->FeatureObservationDuration(
          local_site_data->site_characteristics_for_testing()
              .uses_notifications_in_background());

  local_site_data->NotifySiteUnloaded();

  // Once unloaded the feature observations should still be accessible.
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_IN_USE,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_USAGE_UNKNOWN,
            local_site_data->UsesNotificationsInBackground());

  // Advancing the clock shouldn't affect the observation duration for this
  // feature.
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(observation_duration_before_unload,
            local_site_data->FeatureObservationDuration(
                local_site_data->site_characteristics_for_testing()
                    .uses_notifications_in_background()));
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_USAGE_UNKNOWN,
            local_site_data->UsesNotificationsInBackground());

  local_site_data->NotifySiteLoaded();

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_IN_USE,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(SiteFeatureUsage::SITE_FEATURE_NOT_IN_USE,
            local_site_data->UsesNotificationsInBackground());

  local_site_data->NotifySiteUnloaded();
}

TEST_F(LocalSiteCharacteristicsDataImplTest, AllDurationGetSavedOnUnload) {
  // This test helps making sure that the observation/timestamp fields get saved
  // for all the features being tracked.
  auto local_site_data =
      base::MakeRefCounted<TestLocalSiteCharacteristicsDataImpl>(kDummyOrigin);

  const base::TimeDelta kInterval = base::TimeDelta::FromSeconds(1);
  const auto kIntervalInternalRepresentation =
      TestLocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation(
          kInterval);
  const auto kZeroIntervalInternalRepresentation =
      TestLocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation(
          base::TimeDelta());

  // The internal representation of a zero interval is expected to be equal to
  // zero as the protobuf use variable size integers and so storing zero values
  // is really efficient (uses only one bit).
  EXPECT_EQ(0U, kZeroIntervalInternalRepresentation);

  local_site_data->NotifySiteLoaded();
  test_clock_.Advance(kInterval);
  // Makes use of a feature to make sure that the observation timestamps get
  // saved.
  local_site_data->NotifyUsesAudioInBackground();
  local_site_data->NotifySiteUnloaded();

  SiteCharacteristicsProto expected_proto;

  auto expected_last_loaded_time =
      TestLocalSiteCharacteristicsDataImpl::TimeDeltaToInternalRepresentation(
          kInterval + kInitialTimeSinceEpoch);

  expected_proto.set_last_loaded(expected_last_loaded_time);

  // Features that haven't been used should have an observation duration of
  // |kIntervalInternalRepresentation| and an observation timestamp equal to
  // zero.
  SiteCharacteristicsFeatureProto unused_feature_proto;
  unused_feature_proto.set_observation_duration(
      kIntervalInternalRepresentation);
  unused_feature_proto.set_use_timestamp(kZeroIntervalInternalRepresentation);

  expected_proto.mutable_updates_favicon_in_background()->CopyFrom(
      unused_feature_proto);
  expected_proto.mutable_updates_title_in_background()->CopyFrom(
      unused_feature_proto);
  expected_proto.mutable_uses_notifications_in_background()->CopyFrom(
      unused_feature_proto);

  // The audio feature has been used, so its observation duration value should
  // be equal to zero, and its observation timestamp should be equal to the last
  // loaded time in this case (as this feature has been used right before
  // unloading).
  SiteCharacteristicsFeatureProto used_feature_proto;
  used_feature_proto.set_observation_duration(
      kZeroIntervalInternalRepresentation);
  used_feature_proto.set_use_timestamp(expected_last_loaded_time);
  expected_proto.mutable_uses_audio_in_background()->CopyFrom(
      used_feature_proto);

  EXPECT_TRUE(
      local_site_data->site_characteristics_for_testing().IsInitialized());
  EXPECT_EQ(
      expected_proto.SerializeAsString(),
      local_site_data->site_characteristics_for_testing().SerializeAsString());
}

}  // namespace internal
}  // namespace resource_coordinator
