// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/sample_map.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/invalidation/fake_invalidation_service.h"
#include "chrome/browser/policy/cloud/cloud_policy_core.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "chrome/browser/policy/cloud/cloud_policy_service.h"
#include "chrome/browser/policy/cloud/enterprise_metrics.h"
#include "chrome/browser/policy/cloud/mock_cloud_policy_client.h"
#include "chrome/browser/policy/cloud/mock_cloud_policy_store.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"
#include "policy/policy_constants.h"
#include "sync/notifier/invalidation_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class CloudPolicyInvalidatorTest : public testing::Test,
                                   public CloudPolicyInvalidationHandler {
 protected:
  // Policy objects which can be used in tests.
  enum PolicyObject {
    POLICY_OBJECT_NONE,
    POLICY_OBJECT_A,
    POLICY_OBJECT_B
  };

  CloudPolicyInvalidatorTest();

  virtual void SetUp() OVERRIDE;

  virtual void TearDown() OVERRIDE;

  // Starts the invalidator which will be tested.
  void StartInvalidator(bool initialize);
  void StartInvalidator() {
    StartInvalidator(true /* initialize */);
  }

  // Simulates storing a new policy to the policy store.
  // |object| determines which policy object the store will report the
  // invalidator should register for. May be POLICY_OBJECT_NONE for no object.
  // |invalidation_version| determines what invalidation the store will report.
  // |policy_changed| determines whether the store will report that the
  // policy changed.
  // |timestamp| determines the response timestamp the store will report.
  void StorePolicy(
      PolicyObject object,
      int64 invalidation_version,
      bool policy_changed,
      int64 timestamp);
  void StorePolicy(
      PolicyObject object,
      int64 invalidation_version,
      bool policy_changed) {
    StorePolicy(object, invalidation_version, policy_changed, ++timestamp_);
  }
  void StorePolicy(PolicyObject object, int64 invalidation_version) {
    StorePolicy(object, invalidation_version, false);
  }
  void StorePolicy(PolicyObject object) {
    StorePolicy(object, 0);
  }

  // Disables the invalidation service. It is enabled by default.
  void DisableInvalidationService();

  // Enables the invalidation service. It is enabled by default.
  void EnableInvalidationService();

  // Causes the invalidation service to fire an invalidation. Returns an ack
  // handle which be used to verify that the invalidation was acknowledged.
  syncer::AckHandle FireInvalidation(
      PolicyObject object,
      int64 version,
      const std::string& payload);

  // Causes the invalidation service to fire an invalidation with unknown
  // version. Returns an ack handle which be used to verify that the
  // invalidation was acknowledged.
  syncer::AckHandle FireInvalidation(PolicyObject object);

  // Checks the expected value of the currently set invalidation info.
  bool CheckInvalidationInfo(int64 version, const std::string& payload);

  // Checks that the invalidate callback was not called.
  bool CheckInvalidateNotCalled();

  // Checks that the invalidate callback was called within an appropriate
  // timeframe depending on whether the invalidation had unknown version.
  bool CheckInvalidateCalled(bool unknown_version);
  bool CheckInvalidateCalled() {
    return CheckInvalidateCalled(true);
  }

  // Checks that the state changed callback of the invalidation handler was not
  // called.
  bool CheckStateChangedNotCalled();

  // Checks that the state changed callback of the invalidation handler was
  // called with the given state.
  bool CheckStateChangedCalled(bool invalidations_enabled);

  // Determines if the invalidation with the given ack handle has been
  // acknowledged.
  bool IsInvalidationAcknowledged(const syncer::AckHandle& ack_handle);

  // Get the current count for the given metric.
  base::HistogramBase::Count GetCount(MetricPolicyRefresh metric);
  base::HistogramBase::Count GetInvalidationCount(bool with_payload);

  // CloudPolicyInvalidationHandler:
  virtual void SetInvalidationInfo(
      int64 version,
      const std::string& payload) OVERRIDE;
  virtual void InvalidatePolicy() OVERRIDE;
  virtual void OnInvalidatorStateChanged(bool invalidations_enabled) OVERRIDE;

 private:
  // Returns the object id of the given policy object.
  const invalidation::ObjectId& GetPolicyObjectId(PolicyObject object) const;

  // Get histogram samples for the given histogram.
  scoped_ptr<base::HistogramSamples> GetHistogramSamples(
      const std::string& name) const;

  // Objects the invalidator depends on.
  invalidation::FakeInvalidationService invalidation_service_;
  MockCloudPolicyStore store_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  // The invalidator which will be tested.
  scoped_ptr<CloudPolicyInvalidator> invalidator_;

  // The latest invalidation info set by the invalidator.
  int64 invalidation_version_;
  std::string invalidation_payload_;

  // Object ids for the test policy objects.
  invalidation::ObjectId object_id_a_;
  invalidation::ObjectId object_id_b_;

  // Increasing policy timestamp.
  int64 timestamp_;

  // Fake policy values which are alternated to cause the store to report a
  // changed policy.
  const char* policy_value_a_;
  const char* policy_value_b_;

  // The currently used policy value.
  const char* policy_value_cur_;

  // Stores how many times the invalidate callback was called.
  int invalidate_callback_count_;

  // Stores how many times the state change callback was called for each state.
  int state_change_enabled_callback_count_;
  int state_change_disabled_callback_count_;

  // Stores starting histogram counts for kMetricPolicyRefresh.
  scoped_ptr<base::HistogramSamples> refresh_samples_;

  // Stores starting histogram counts for kMetricPolicyInvalidations.
  scoped_ptr<base::HistogramSamples> invalidations_samples_;
};

CloudPolicyInvalidatorTest::CloudPolicyInvalidatorTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      invalidation_version_(0),
      object_id_a_(135, "asdf"),
      object_id_b_(246, "zxcv"),
      timestamp_(123456),
      policy_value_a_("asdf"),
      policy_value_b_("zxcv"),
      policy_value_cur_(policy_value_a_),
      invalidate_callback_count_(0),
      state_change_enabled_callback_count_(0),
      state_change_disabled_callback_count_(0) {}

void CloudPolicyInvalidatorTest::SetUp() {
  base::StatisticsRecorder::Initialize();
  refresh_samples_ = GetHistogramSamples(kMetricPolicyRefresh);
  invalidations_samples_ = GetHistogramSamples(kMetricPolicyInvalidations);
}

void CloudPolicyInvalidatorTest::TearDown() {
  EXPECT_FALSE(invalidation_service_.ReceivedInvalidAcknowledgement());
  if (invalidator_)
    invalidator_->Shutdown();
}

void CloudPolicyInvalidatorTest::StartInvalidator(bool initialize) {
  invalidator_.reset(new CloudPolicyInvalidator(
      this /* invalidation_handler */,
      &store_,
      task_runner_));
  if (initialize)
    invalidator_->InitializeWithService(&invalidation_service_);
}

void CloudPolicyInvalidatorTest::StorePolicy(
    PolicyObject object,
    int64 invalidation_version,
    bool policy_changed,
    int64 timestamp) {
  enterprise_management::PolicyData* data =
      new enterprise_management::PolicyData();
  if (object != POLICY_OBJECT_NONE) {
    data->set_invalidation_source(GetPolicyObjectId(object).source());
    data->set_invalidation_name(GetPolicyObjectId(object).name());
  }
  data->set_timestamp(timestamp);
  // Swap the policy value if a policy change is desired.
  if (policy_changed)
    policy_value_cur_ = policy_value_cur_ == policy_value_a_ ?
        policy_value_b_ : policy_value_a_;
  data->set_policy_value(policy_value_cur_);
  store_.invalidation_version_ = invalidation_version;
  store_.policy_.reset(data);
  base::DictionaryValue policies;
  policies.SetInteger(
      key::kMaxInvalidationFetchDelay,
      CloudPolicyInvalidator::kMaxFetchDelayMin);
  store_.policy_map_.LoadFrom(
      &policies,
      POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_MACHINE);
  store_.NotifyStoreLoaded();
}

void CloudPolicyInvalidatorTest::DisableInvalidationService() {
  invalidation_service_.SetInvalidatorState(
      syncer::TRANSIENT_INVALIDATION_ERROR);
}

void CloudPolicyInvalidatorTest::EnableInvalidationService() {
  invalidation_service_.SetInvalidatorState(syncer::INVALIDATIONS_ENABLED);
}

syncer::AckHandle CloudPolicyInvalidatorTest::FireInvalidation(
    PolicyObject object,
    int64 version,
    const std::string& payload) {
  return invalidation_service_.EmitInvalidationForTest(
      GetPolicyObjectId(object),
      version,
      payload);
}

syncer::AckHandle CloudPolicyInvalidatorTest::FireInvalidation(
    PolicyObject object) {
  return invalidation_service_.EmitInvalidationForTest(
      GetPolicyObjectId(object),
      syncer::Invalidation::kUnknownVersion,
      std::string());
}

bool CloudPolicyInvalidatorTest::CheckInvalidationInfo(
    int64 version,
    const std::string& payload) {
  return version == invalidation_version_ && payload == invalidation_payload_;
}

bool CloudPolicyInvalidatorTest::CheckInvalidateNotCalled() {
  bool result = true;
  if (invalidate_callback_count_ != 0)
    result = false;
  task_runner_->RunUntilIdle();
  if (invalidate_callback_count_ != 0)
    result = false;
  return result;
}

bool CloudPolicyInvalidatorTest::CheckInvalidateCalled(bool unknown_version) {
  base::TimeDelta min_delay;
  base::TimeDelta max_delay = base::TimeDelta::FromMilliseconds(
      CloudPolicyInvalidator::kMaxFetchDelayMin);
  if (unknown_version) {
    base::TimeDelta additional_delay = base::TimeDelta::FromMinutes(
        CloudPolicyInvalidator::kMissingPayloadDelay);
    min_delay += additional_delay;
    max_delay += additional_delay;
  }

  if (task_runner_->GetPendingTasks().empty())
    return false;
  base::TimeDelta actual_delay = task_runner_->GetPendingTasks().back().delay;
  EXPECT_GE(actual_delay, min_delay);
  EXPECT_LE(actual_delay, max_delay);

  bool result = true;
  if (invalidate_callback_count_ != 0)
    result = false;
  task_runner_->RunUntilIdle();
  if (invalidate_callback_count_ != 1)
    result = false;
  invalidate_callback_count_ = 0;
  return result;
}

bool CloudPolicyInvalidatorTest::CheckStateChangedNotCalled() {
  return state_change_enabled_callback_count_ == 0 &&
      state_change_disabled_callback_count_ == 0;
}

bool CloudPolicyInvalidatorTest::CheckStateChangedCalled(
    bool invalidations_enabled) {
  int expected_enabled_count_ = invalidations_enabled ? 1 : 0;
  int expected_disabled_count_ = invalidations_enabled ? 0 : 1;
  bool result = state_change_enabled_callback_count_ == expected_enabled_count_
      && state_change_disabled_callback_count_ == expected_disabled_count_;
  state_change_enabled_callback_count_ = 0;
  state_change_disabled_callback_count_ = 0;
  return result;
}

bool CloudPolicyInvalidatorTest::IsInvalidationAcknowledged(
    const syncer::AckHandle& ack_handle) {
  return invalidation_service_.IsInvalidationAcknowledged(ack_handle);
}

base::HistogramBase::Count CloudPolicyInvalidatorTest::GetCount(
    MetricPolicyRefresh metric) {
  return GetHistogramSamples(kMetricPolicyRefresh)->GetCount(metric) -
      refresh_samples_->GetCount(metric);
}

base::HistogramBase::Count CloudPolicyInvalidatorTest::GetInvalidationCount(
    bool with_payload) {
  int metric = with_payload ? 1 : 0;
  return GetHistogramSamples(kMetricPolicyInvalidations)->GetCount(metric) -
      invalidations_samples_->GetCount(metric);
}

void CloudPolicyInvalidatorTest::SetInvalidationInfo(
    int64 version,
    const std::string& payload) {
  invalidation_version_ = version;
  invalidation_payload_ = payload;
}

void CloudPolicyInvalidatorTest::InvalidatePolicy() {
  ++invalidate_callback_count_;
}

void CloudPolicyInvalidatorTest::OnInvalidatorStateChanged(
    bool invalidations_enabled) {
  if (invalidator_.get())
    EXPECT_EQ(invalidations_enabled, invalidator_->invalidations_enabled());
  if (invalidations_enabled)
    ++state_change_enabled_callback_count_;
  else
    ++state_change_disabled_callback_count_;
}

const invalidation::ObjectId& CloudPolicyInvalidatorTest::GetPolicyObjectId(
    PolicyObject object) const {
  EXPECT_TRUE(object == POLICY_OBJECT_A || object == POLICY_OBJECT_B);
  return object == POLICY_OBJECT_A ? object_id_a_ : object_id_b_;
}

scoped_ptr<base::HistogramSamples>
    CloudPolicyInvalidatorTest::GetHistogramSamples(
        const std::string& name) const {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return scoped_ptr<base::HistogramSamples>(new base::SampleMap());
  return histogram->SnapshotSamples();
}

TEST_F(CloudPolicyInvalidatorTest, Uninitialized) {
  // No invalidations should be processed if the invalidator is not intialized.
  StartInvalidator(false /* initialize */);
  StorePolicy(POLICY_OBJECT_A);
  FireInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidateNotCalled());
}

TEST_F(CloudPolicyInvalidatorTest, RegisterOnStoreLoaded) {
  // No registration when store is not loaded.
  StartInvalidator();
  EXPECT_TRUE(CheckStateChangedNotCalled());
  FireInvalidation(POLICY_OBJECT_A);
  FireInvalidation(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckInvalidateNotCalled());

  // No registration when store is loaded with no invalidation object id.
  StorePolicy(POLICY_OBJECT_NONE);
  EXPECT_TRUE(CheckStateChangedNotCalled());
  FireInvalidation(POLICY_OBJECT_A);
  FireInvalidation(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckInvalidateNotCalled());

  // Check registration when store is loaded for object A.
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckStateChangedCalled(true));
  FireInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidateCalled());
  FireInvalidation(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckInvalidateNotCalled());
}

TEST_F(CloudPolicyInvalidatorTest, ChangeRegistration) {
  // Register for object A.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckStateChangedCalled(true));
  FireInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidateCalled());
  FireInvalidation(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckInvalidateNotCalled());
  syncer::AckHandle ack = FireInvalidation(POLICY_OBJECT_A);

  // Check re-registration for object B. Make sure the pending invalidation for
  // object A is acknowledged without making the callback.
  StorePolicy(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckStateChangedNotCalled());
  EXPECT_TRUE(IsInvalidationAcknowledged(ack));
  EXPECT_TRUE(CheckInvalidateNotCalled());

  // Make sure future invalidations for object A are ignored and for object B
  // are processed.
  FireInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidateNotCalled());
  FireInvalidation(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckInvalidateCalled());
}

TEST_F(CloudPolicyInvalidatorTest, UnregisterOnStoreLoaded) {
  // Register for object A.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckStateChangedCalled(true));
  FireInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidateCalled());

  // Check unregistration when store is loaded with no invalidation object id.
  syncer::AckHandle ack = FireInvalidation(POLICY_OBJECT_A);
  EXPECT_FALSE(IsInvalidationAcknowledged(ack));
  StorePolicy(POLICY_OBJECT_NONE);
  EXPECT_TRUE(IsInvalidationAcknowledged(ack));
  EXPECT_TRUE(CheckStateChangedCalled(false));
  FireInvalidation(POLICY_OBJECT_A);
  FireInvalidation(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckInvalidateNotCalled());

  // Check re-registration for object B.
  StorePolicy(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckStateChangedCalled(true));
  FireInvalidation(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckInvalidateCalled());
}

TEST_F(CloudPolicyInvalidatorTest, HandleInvalidation) {
  // Register and fire invalidation
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  EXPECT_TRUE(CheckStateChangedCalled(true));
  syncer::AckHandle ack = FireInvalidation(POLICY_OBJECT_A, 12, "test_payload");

  // Make sure client info is set as soon as the invalidation is received.
  EXPECT_TRUE(CheckInvalidationInfo(12, "test_payload"));
  EXPECT_TRUE(CheckInvalidateCalled(false /* unknown_version */));

  // Make sure invalidation is not acknowledged until the store is loaded.
  EXPECT_FALSE(IsInvalidationAcknowledged(ack));
  EXPECT_TRUE(CheckInvalidationInfo(12, "test_payload"));
  StorePolicy(POLICY_OBJECT_A, 12);
  EXPECT_TRUE(IsInvalidationAcknowledged(ack));
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
}

TEST_F(CloudPolicyInvalidatorTest, HandleInvalidationWithUnknownVersion) {
  // Register and fire invalidation with unknown version.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  syncer::AckHandle ack = FireInvalidation(POLICY_OBJECT_A);

  // Make sure client info is not set until after the invalidation callback is
  // made.
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(CheckInvalidateCalled());
  EXPECT_TRUE(CheckInvalidationInfo(-1, std::string()));

  // Make sure invalidation is not acknowledged until the store is loaded.
  EXPECT_FALSE(IsInvalidationAcknowledged(ack));
  StorePolicy(POLICY_OBJECT_A, -1);
  EXPECT_TRUE(IsInvalidationAcknowledged(ack));
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
}

TEST_F(CloudPolicyInvalidatorTest, HandleMultipleInvalidations) {
  // Generate multiple invalidations.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  syncer::AckHandle ack1 = FireInvalidation(POLICY_OBJECT_A, 1, "test1");
  EXPECT_TRUE(CheckInvalidationInfo(1, "test1"));
  syncer::AckHandle ack2 = FireInvalidation(POLICY_OBJECT_A, 2, "test2");
  EXPECT_TRUE(CheckInvalidationInfo(2, "test2"));
  syncer::AckHandle ack3= FireInvalidation(POLICY_OBJECT_A, 3, "test3");
  EXPECT_TRUE(CheckInvalidationInfo(3, "test3"));

  // Make sure the replaced invalidations are acknowledged.
  EXPECT_TRUE(IsInvalidationAcknowledged(ack1));
  EXPECT_TRUE(IsInvalidationAcknowledged(ack2));

  // Make sure the invalidate callback is called once.
  EXPECT_TRUE(CheckInvalidateCalled(false /* unknown_version */));

  // Make sure that the last invalidation is only acknowledged after the store
  // is loaded with the latest version.
  StorePolicy(POLICY_OBJECT_A, 1);
  EXPECT_FALSE(IsInvalidationAcknowledged(ack3));
  StorePolicy(POLICY_OBJECT_A, 2);
  EXPECT_FALSE(IsInvalidationAcknowledged(ack3));
  StorePolicy(POLICY_OBJECT_A, 3);
  EXPECT_TRUE(IsInvalidationAcknowledged(ack3));
}

TEST_F(CloudPolicyInvalidatorTest,
       HandleMultipleInvalidationsWithUnknownVersion) {
  // Validate that multiple invalidations with unknown version each generate
  // unique invalidation version numbers.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  syncer::AckHandle ack1 = FireInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(CheckInvalidateCalled());
  EXPECT_TRUE(CheckInvalidationInfo(-1, std::string()));
  syncer::AckHandle ack2 = FireInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(CheckInvalidateCalled());
  EXPECT_TRUE(CheckInvalidationInfo(-2, std::string()));
  syncer::AckHandle ack3 = FireInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(CheckInvalidateCalled());
  EXPECT_TRUE(CheckInvalidationInfo(-3, std::string()));

  // Make sure the replaced invalidations are acknowledged.
  EXPECT_TRUE(IsInvalidationAcknowledged(ack1));
  EXPECT_TRUE(IsInvalidationAcknowledged(ack2));

  // Make sure that the last invalidation is only acknowledged after the store
  // is loaded with the last unknown version.
  StorePolicy(POLICY_OBJECT_A, -1);
  EXPECT_FALSE(IsInvalidationAcknowledged(ack3));
  StorePolicy(POLICY_OBJECT_A, -2);
  EXPECT_FALSE(IsInvalidationAcknowledged(ack3));
  StorePolicy(POLICY_OBJECT_A, -3);
  EXPECT_TRUE(IsInvalidationAcknowledged(ack3));
}

TEST_F(CloudPolicyInvalidatorTest, AcknowledgeBeforeInvalidateCallback) {
  // Generate an invalidation.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  syncer::AckHandle ack = FireInvalidation(POLICY_OBJECT_A, 3, "test");

  // Ensure that the invalidate callback is not made and the invalidation is
  // acknowledged if the store is loaded with the latest version before the
  // callback is invoked.
  StorePolicy(POLICY_OBJECT_A, 3);
  EXPECT_TRUE(IsInvalidationAcknowledged(ack));
  EXPECT_TRUE(CheckInvalidateNotCalled());
}

TEST_F(CloudPolicyInvalidatorTest, StateChanged) {
  // Before registration, changes to the invalidation service state should not
  // generate change state notifications.
  StartInvalidator();
  DisableInvalidationService();
  EnableInvalidationService();
  EXPECT_TRUE(CheckStateChangedNotCalled());

  // After registration, changes to the invalidation service state should
  // generate notifications.
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckStateChangedCalled(true));
  DisableInvalidationService();
  EXPECT_TRUE(CheckStateChangedCalled(false));
  DisableInvalidationService();
  EXPECT_TRUE(CheckStateChangedNotCalled());
  EnableInvalidationService();
  EXPECT_TRUE(CheckStateChangedCalled(true));
  EnableInvalidationService();
  EXPECT_TRUE(CheckStateChangedNotCalled());

  // When the invalidation service is enabled, changes to the registration
  // state should generate notifications.
  StorePolicy(POLICY_OBJECT_NONE);
  EXPECT_TRUE(CheckStateChangedCalled(false));
  StorePolicy(POLICY_OBJECT_NONE);
  EXPECT_TRUE(CheckStateChangedNotCalled());
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckStateChangedCalled(true));
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckStateChangedNotCalled());

  // When the invalidation service is disabled, changes to the registration
  // state should not generate notifications.
  DisableInvalidationService();
  EXPECT_TRUE(CheckStateChangedCalled(false));
  StorePolicy(POLICY_OBJECT_NONE);
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckStateChangedNotCalled());
}

TEST_F(CloudPolicyInvalidatorTest, RefreshMetricsUnregistered) {
  // Store loads occurring before invalidation registration are not counted.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_NONE, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_NONE, 0, true /* policy_changed */);
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));
}

TEST_F(CloudPolicyInvalidatorTest, RefreshMetricsNoInvalidations) {
  // Store loads occurring while registered should be differentiated depending
  // on whether the invalidation service was enabled or not.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  DisableInvalidationService();
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(3, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));
}

TEST_F(CloudPolicyInvalidatorTest, RefreshMetricsStoreSameTimestamp) {
  // Store loads with the same timestamp as the load which causes registration
  // are not counted.
  StartInvalidator();
  StorePolicy(
      POLICY_OBJECT_A, 0, false /* policy_changed */, 12 /* timestamp */);
  StorePolicy(
      POLICY_OBJECT_A, 0, false /* policy_changed */, 12 /* timestamp */);
  StorePolicy(
      POLICY_OBJECT_A, 0, true /* policy_changed */, 12 /* timestamp */);

  // The next load with a different timestamp counts.
  StorePolicy(
      POLICY_OBJECT_A, 0, true /* policy_changed */, 13 /* timestamp */);

  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));
}

TEST_F(CloudPolicyInvalidatorTest, RefreshMetricsInvalidation) {
  // Store loads after an invalidation are counted as invalidated, even if
  // the loads do not result in the invalidation being acknowledged.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_A);
  FireInvalidation(POLICY_OBJECT_A, 5, "test");
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 5, true /* policy_changed */);

  // Store loads after the invalidation is complete are not counted as
  // invalidated.
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);

  EXPECT_EQ(3, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));
}

TEST_F(CloudPolicyInvalidatorTest, InvalidationMetrics) {
  // Generate a mix of versioned and unknown-version invalidations.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  FireInvalidation(POLICY_OBJECT_B);
  FireInvalidation(POLICY_OBJECT_A);
  FireInvalidation(POLICY_OBJECT_B, 1, "test");
  FireInvalidation(POLICY_OBJECT_A, 1, "test");
  FireInvalidation(POLICY_OBJECT_A, 2, "test");
  FireInvalidation(POLICY_OBJECT_A);
  FireInvalidation(POLICY_OBJECT_A);
  FireInvalidation(POLICY_OBJECT_A, 3, "test");
  FireInvalidation(POLICY_OBJECT_A, 4, "test");

  // Verify that received invalidations metrics are correct.
  EXPECT_EQ(3, GetInvalidationCount(false /* with_payload */));
  EXPECT_EQ(4, GetInvalidationCount(true /* with_payload */));
}

}  // namespace policy
