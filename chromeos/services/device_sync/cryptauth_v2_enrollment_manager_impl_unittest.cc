// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/base64url.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/timer/mock_timer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/services/device_sync/cryptauth_constants.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_scheduler.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/services/device_sync/cryptauth_v2_enroller.h"
#include "chromeos/services/device_sync/cryptauth_v2_enroller_impl.h"
#include "chromeos/services/device_sync/cryptauth_v2_enrollment_manager_impl.h"
#include "chromeos/services/device_sync/fake_cryptauth_enrollment_scheduler.h"
#include "chromeos/services/device_sync/fake_cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/fake_cryptauth_v2_enroller.h"
#include "chromeos/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/services/device_sync/network_aware_enrollment_scheduler.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_better_together_feature_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_client_app_metadata_provider.h"
#include "chromeos/services/device_sync/public/cpp/gcm_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kFakeV1PublicKey[] = "public_key_v1";
const char kFakeV1PrivateKey[] = "private_key_v1";
const char kFakeV2PublicKey[] = "public_key_v2";
const char kFakeV2PrivateKey[] = "private_key_v2";

const base::TimeDelta kFakeRefreshPeriod = base::TimeDelta::FromMilliseconds(
    cryptauthv2::kTestClientDirectiveCheckinDelayMillis);
const base::TimeDelta kFakeRetryPeriod = base::TimeDelta::FromMilliseconds(
    cryptauthv2::kTestClientDirectiveRetryPeriodMillis);
const base::Time kFakeLastEnrollmentTime = base::Time::UnixEpoch();
const base::Time kFakeTimeAfterRefreshPeriod = kFakeLastEnrollmentTime +
                                               kFakeRefreshPeriod +
                                               base::TimeDelta::FromSeconds(1);
const base::Time kFakeTimeBeforeRefreshPeriod = kFakeLastEnrollmentTime +
                                                kFakeRefreshPeriod -
                                                base::TimeDelta::FromSeconds(1);

// A child of FakeCryptAuthEnrollmentScheduler that sets scheduler parameters
// based on the latest enrollment result.
class FakeCryptAuthEnrollmentSchedulerWithResultHandling
    : public FakeCryptAuthEnrollmentScheduler {
 public:
  FakeCryptAuthEnrollmentSchedulerWithResultHandling(
      CryptAuthEnrollmentScheduler::Delegate* delegate,
      base::Clock* clock)
      : FakeCryptAuthEnrollmentScheduler(delegate), clock_(clock) {}

  ~FakeCryptAuthEnrollmentSchedulerWithResultHandling() override = default;

  void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) override {
    FakeCryptAuthEnrollmentScheduler::HandleEnrollmentResult(enrollment_result);

    if (handled_enrollment_results().back().IsSuccess()) {
      set_num_consecutive_failures(0);
      set_time_to_next_enrollment_request(kFakeRefreshPeriod);
      set_last_successful_enrollment_time(clock_->Now());
    } else {
      set_num_consecutive_failures(GetNumConsecutiveFailures() + 1);
      set_time_to_next_enrollment_request(kFakeRetryPeriod);
    }
  }

 private:
  base::Clock* clock_;
};

class FakeNetworkAwareEnrollmentSchedulerFactory
    : public NetworkAwareEnrollmentScheduler::Factory {
 public:
  FakeNetworkAwareEnrollmentSchedulerFactory(
      const PrefService* expected_pref_service,
      base::Clock* clock)
      : expected_pref_service_(expected_pref_service), clock_(clock) {}

  ~FakeNetworkAwareEnrollmentSchedulerFactory() override = default;

  FakeCryptAuthEnrollmentSchedulerWithResultHandling* instance() {
    return instance_;
  }

  void SetInitialNumConsecutiveFailures(size_t num_consecutive_failures) {
    initial_num_consecutive_failures_ = num_consecutive_failures;
  }

 private:
  // NetworkAwareEnrollmentScheduler::Factory
  std::unique_ptr<CryptAuthEnrollmentScheduler> BuildInstance(
      CryptAuthEnrollmentScheduler::Delegate* delegate,
      PrefService* pref_service,
      NetworkStateHandler* network_state_handler) override {
    EXPECT_EQ(expected_pref_service_, pref_service);

    auto instance =
        std::make_unique<FakeCryptAuthEnrollmentSchedulerWithResultHandling>(
            delegate, clock_);
    instance_ = instance.get();

    if (initial_num_consecutive_failures_) {
      instance->set_num_consecutive_failures(
          *initial_num_consecutive_failures_);
    }

    return instance;
  }

  const PrefService* expected_pref_service_;
  base::Clock* clock_;

  base::Optional<size_t> initial_num_consecutive_failures_;

  FakeCryptAuthEnrollmentSchedulerWithResultHandling* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeNetworkAwareEnrollmentSchedulerFactory);
};

class FakeCryptAuthV2EnrollerFactory : public CryptAuthV2EnrollerImpl::Factory {
 public:
  FakeCryptAuthV2EnrollerFactory(
      const CryptAuthKeyRegistry* expected_key_registry,
      const CryptAuthClientFactory* expected_client_factory)
      : expected_key_registry_(expected_key_registry),
        expected_client_factory_(expected_client_factory) {}

  ~FakeCryptAuthV2EnrollerFactory() override = default;

  const std::vector<FakeCryptAuthV2Enroller*>& created_instances() {
    return created_instances_;
  }

 private:
  // CryptAuthV2EnrollerImpl::Factory:
  std::unique_ptr<CryptAuthV2Enroller> BuildInstance(
      CryptAuthKeyRegistry* key_registry,
      CryptAuthClientFactory* client_factory,
      std::unique_ptr<base::OneShotTimer> timer =
          std::make_unique<base::MockOneShotTimer>()) override {
    EXPECT_EQ(expected_key_registry_, key_registry);
    EXPECT_EQ(expected_client_factory_, client_factory);

    auto instance = std::make_unique<FakeCryptAuthV2Enroller>();
    created_instances_.push_back(instance.get());

    return instance;
  }

  const CryptAuthKeyRegistry* expected_key_registry_;
  const CryptAuthClientFactory* expected_client_factory_;

  std::vector<FakeCryptAuthV2Enroller*> created_instances_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthV2EnrollerFactory);
};

}  // namespace

class DeviceSyncCryptAuthV2EnrollmentManagerImplTest
    : public testing::Test,
      CryptAuthEnrollmentManager::Observer {
 protected:
  DeviceSyncCryptAuthV2EnrollmentManagerImplTest()
      : fake_gcm_manager_(std::string() /* registration_id */),
        mock_client_factory_(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS) {}

  // testing::Test:
  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();

    CryptAuthV2EnrollmentManagerImpl::RegisterPrefs(
        test_pref_service_.registry());
    CryptAuthKeyRegistryImpl::RegisterPrefs(test_pref_service_.registry());

    key_registry_ = CryptAuthKeyRegistryImpl::Factory::Get()->BuildInstance(
        &test_pref_service_);

    fake_enrollment_scheduler_factory_ =
        std::make_unique<FakeNetworkAwareEnrollmentSchedulerFactory>(
            &test_pref_service_, &test_clock_);
    NetworkAwareEnrollmentScheduler::Factory::SetFactoryForTesting(
        fake_enrollment_scheduler_factory_.get());

    fake_enroller_factory_ = std::make_unique<FakeCryptAuthV2EnrollerFactory>(
        key_registry_.get(), &mock_client_factory_);
    CryptAuthV2EnrollerImpl::Factory::SetFactoryForTesting(
        fake_enroller_factory_.get());
  }

  // testing::Test:
  void TearDown() override {
    if (enrollment_manager())
      enrollment_manager()->RemoveObserver(this);

    NetworkAwareEnrollmentScheduler::Factory::SetFactoryForTesting(nullptr);
    CryptAuthV2EnrollerImpl::Factory::SetFactoryForTesting(nullptr);

    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

  void AddV1UserKeyPairToV1Prefs(const std::string& public_key,
                                 const std::string& private_key) {
    std::string public_key_b64, private_key_b64;
    base::Base64UrlEncode(public_key,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &public_key_b64);
    base::Base64UrlEncode(private_key,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &private_key_b64);

    test_pref_service_.SetString(prefs::kCryptAuthEnrollmentUserPublicKey,
                                 public_key_b64);
    test_pref_service_.SetString(prefs::kCryptAuthEnrollmentUserPrivateKey,
                                 private_key_b64);
  }

  void SetInitialNumConsecutiveFailures(size_t num_consecutive_failures) {
    num_consecutive_failures_ = num_consecutive_failures;
    fake_enrollment_scheduler_factory_->SetInitialNumConsecutiveFailures(
        num_consecutive_failures);
  }

  // CryptAuthEnrollmentManager::Observer:
  void OnEnrollmentStarted() override {
    ++num_enrollment_started_notifications_;

    enrollment_finished_success_ = base::nullopt;
  }
  void OnEnrollmentFinished(bool success) override {
    enrollment_finished_success_ = success;
  }

  void CreateEnrollmentManager() {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    enrollment_manager_ =
        CryptAuthV2EnrollmentManagerImpl::Factory::Get()->BuildInstance(
            &fake_client_app_metadata_provider_, key_registry_.get(),
            &mock_client_factory_, &fake_gcm_manager_, &test_pref_service_,
            &test_clock_, std::move(mock_timer));

    enrollment_manager()->AddObserver(this);
  }

  void VerifyEnrollmentManagerObserversNotifiedOfStart(
      size_t expected_num_enrollment_started_notifications) {
    EXPECT_EQ(expected_num_enrollment_started_notifications,
              num_enrollment_started_notifications_);
  }

  void VerifyPersistedFailureRecoveryInvocationReason(
      const cryptauthv2::ClientMetadata::InvocationReason&
          expected_failure_recovery_invocation_reason) {
    EXPECT_EQ(
        expected_failure_recovery_invocation_reason,
        static_cast<cryptauthv2::ClientMetadata::InvocationReason>(
            test_pref_service_.GetInteger(
                prefs::kCryptAuthEnrollmentFailureRecoveryInvocationReason)));
  }

  void VerifyEnrollmentResult(
      const CryptAuthEnrollmentResult& expected_result) {
    VerifyResultSentToEnrollmentManagerObservers(expected_result.IsSuccess());
    EXPECT_EQ(expected_result,
              fake_enrollment_scheduler()->handled_enrollment_results().back());
  }

  TestingPrefServiceSimple* test_pref_service() { return &test_pref_service_; }

  CryptAuthKeyRegistry* key_registry() { return key_registry_.get(); }

  FakeCryptAuthEnrollmentSchedulerWithResultHandling*
  fake_enrollment_scheduler() {
    return fake_enrollment_scheduler_factory_->instance();
  }

  FakeCryptAuthGCMManager* fake_gcm_manager() { return &fake_gcm_manager_; }

  base::SimpleTestClock* test_clock() { return &test_clock_; }

  base::MockOneShotTimer* mock_timer() { return mock_timer_; }

  FakeClientAppMetadataProvider* fake_client_app_metadata_provider() {
    return &fake_client_app_metadata_provider_;
  }

  MockCryptAuthClientFactory* mock_client_factory() {
    return &mock_client_factory_;
  }

  FakeCryptAuthV2Enroller* fake_enroller(size_t index = 0u) {
    // Only the most recently created enroller is valid.
    EXPECT_EQ(fake_enroller_factory_->created_instances().size() - 1, index);

    return fake_enroller_factory_->created_instances()[index];
  }

  CryptAuthEnrollmentManager* enrollment_manager() {
    return enrollment_manager_.get();
  }

 private:
  void VerifyResultSentToEnrollmentManagerObservers(bool success) {
    EXPECT_EQ(success, enrollment_finished_success_);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  size_t num_enrollment_started_notifications_ = 0;
  base::Optional<bool> enrollment_finished_success_ = base::nullopt;
  size_t num_consecutive_failures_ = 0;

  TestingPrefServiceSimple test_pref_service_;
  FakeClientAppMetadataProvider fake_client_app_metadata_provider_;
  FakeCryptAuthGCMManager fake_gcm_manager_;
  base::SimpleTestClock test_clock_;
  base::MockOneShotTimer* mock_timer_;
  MockCryptAuthClientFactory mock_client_factory_;
  std::unique_ptr<CryptAuthKeyRegistry> key_registry_;
  std::unique_ptr<FakeNetworkAwareEnrollmentSchedulerFactory>
      fake_enrollment_scheduler_factory_;
  std::unique_ptr<FakeCryptAuthV2EnrollerFactory> fake_enroller_factory_;

  std::unique_ptr<CryptAuthEnrollmentManager> enrollment_manager_;
};

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       EnrollmentRequestedFromScheduler_NeverPreviouslyEnrolled) {
  CreateEnrollmentManager();
  EXPECT_FALSE(fake_enrollment_scheduler());

  enrollment_manager()->Start();
  EXPECT_TRUE(fake_enrollment_scheduler());

  // The user has never enrolled with v1 or v2 and has not registered with GCM.
  EXPECT_TRUE(key_registry()->enrolled_key_bundles().empty());
  EXPECT_TRUE(fake_gcm_manager()->GetRegistrationId().empty());
  EXPECT_TRUE(enrollment_manager()->GetLastEnrollmentTime().is_null());
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentValid());

  cryptauthv2::PolicyReference expected_policy_reference =
      cryptauthv2::BuildPolicyReference("expected_policy_reference_name",
                                        1 /* version */);
  fake_enrollment_scheduler()->set_client_directive_policy_reference(
      expected_policy_reference);
  fake_enrollment_scheduler()->RequestEnrollmentNow();
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      1 /* expected_num_enrollment_started_notifications */);

  fake_gcm_manager()->CompleteRegistration(cryptauthv2::kTestGcmRegistrationId);

  EXPECT_EQ(1u,
            fake_client_app_metadata_provider()->metadata_requests().size());
  EXPECT_EQ(cryptauthv2::kTestGcmRegistrationId,
            fake_client_app_metadata_provider()
                ->metadata_requests()[0]
                .gcm_registration_id);
  std::move(
      fake_client_app_metadata_provider()->metadata_requests()[0].callback)
      .Run(cryptauthv2::GetClientAppMetadataForTest());

  EXPECT_TRUE(fake_enroller()->was_enroll_called());
  EXPECT_EQ(
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */,
          cryptauthv2::ClientMetadata::INITIALIZATION /* invocation_reason */)
          .SerializeAsString(),
      fake_enroller()->client_metadata()->SerializeAsString());
  EXPECT_EQ(cryptauthv2::GetClientAppMetadataForTest().SerializeAsString(),
            fake_enroller()->client_app_metadata()->SerializeAsString());
  EXPECT_EQ(expected_policy_reference.SerializeAsString(),
            (*fake_enroller()->client_directive_policy_reference())
                ->SerializeAsString());

  CryptAuthEnrollmentResult expected_enrollment_result(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      cryptauthv2::GetClientDirectiveForTest());
  fake_enroller()->FinishAttempt(expected_enrollment_result);
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentInProgress());

  VerifyEnrollmentResult(expected_enrollment_result);
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest, GcmRegistrationFailed) {
  CreateEnrollmentManager();
  enrollment_manager()->Start();
  fake_enrollment_scheduler()->RequestEnrollmentNow();

  // An empty registration ID is interpreted as an error by
  // FakeCryptAuthGCMManager.
  fake_gcm_manager()->CompleteRegistration(std::string() /* registration_id */);

  VerifyEnrollmentResult(CryptAuthEnrollmentResult(
      CryptAuthEnrollmentResult::ResultCode::kErrorGcmRegistrationFailed,
      base::nullopt /* client_directive */));
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest, GcmRegistrationTimeout) {
  CreateEnrollmentManager();
  enrollment_manager()->Start();
  fake_enrollment_scheduler()->RequestEnrollmentNow();

  // Timeout waiting for GcmRegistration.
  EXPECT_TRUE(mock_timer()->IsRunning());
  mock_timer()->Fire();

  VerifyEnrollmentResult(
      CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                    kErrorTimeoutWaitingForGcmRegistration,
                                base::nullopt /* client_directive */));
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       ClientAppMetadataFetchFailed) {
  CreateEnrollmentManager();
  enrollment_manager()->Start();
  fake_enrollment_scheduler()->RequestEnrollmentNow();
  fake_gcm_manager()->CompleteRegistration(cryptauthv2::kTestGcmRegistrationId);

  // A failed metadata retrieval will return base::nullopt.
  std::move(
      fake_client_app_metadata_provider()->metadata_requests()[0].callback)
      .Run(base::nullopt /* client_app_metadata */);

  VerifyEnrollmentResult(CryptAuthEnrollmentResult(
      CryptAuthEnrollmentResult::ResultCode::kErrorClientAppMetadataFetchFailed,
      base::nullopt /* client_directive */));
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       ClientAppMetadataTimeout) {
  CreateEnrollmentManager();
  enrollment_manager()->Start();
  fake_enrollment_scheduler()->RequestEnrollmentNow();
  fake_gcm_manager()->CompleteRegistration(cryptauthv2::kTestGcmRegistrationId);

  // Timeout waiting for ClientAppMetadata.
  EXPECT_TRUE(mock_timer()->IsRunning());
  mock_timer()->Fire();

  VerifyEnrollmentResult(
      CryptAuthEnrollmentResult(CryptAuthEnrollmentResult::ResultCode::
                                    kErrorTimeoutWaitingForClientAppMetadata,
                                base::nullopt /* client_directive */));
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest, ForcedEnrollment) {
  CreateEnrollmentManager();
  enrollment_manager()->Start();

  enrollment_manager()->ForceEnrollmentNow(
      cryptauth::InvocationReason::INVOCATION_REASON_FEATURE_TOGGLED);
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      1 /* expected_num_enrollment_started_notifications */);

  // Simulate a failed enrollment attempt due to CryptAuth server overload.
  fake_gcm_manager()->CompleteRegistration(cryptauthv2::kTestGcmRegistrationId);
  std::move(
      fake_client_app_metadata_provider()->metadata_requests()[0].callback)
      .Run(cryptauthv2::GetClientAppMetadataForTest());
  CryptAuthEnrollmentResult expected_enrollment_result(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      base::nullopt /* client_directive */);
  fake_enroller()->FinishAttempt(expected_enrollment_result);
  VerifyEnrollmentResult(expected_enrollment_result);
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       RetryAfterFailedPeriodicEnrollment_PreviouslyEnrolled) {
  CreateEnrollmentManager();
  enrollment_manager()->Start();

  // The user has already enrolled and is due for a refresh.
  base::Time expected_last_enrollment_time = kFakeLastEnrollmentTime;
  fake_enrollment_scheduler()->set_last_successful_enrollment_time(
      expected_last_enrollment_time);
  fake_enrollment_scheduler()->set_refresh_period(kFakeRefreshPeriod);
  test_clock()->SetNow(kFakeTimeAfterRefreshPeriod);
  base::TimeDelta expected_time_to_next_attempt =
      base::TimeDelta::FromSeconds(0);
  fake_enrollment_scheduler()->set_time_to_next_enrollment_request(
      expected_time_to_next_attempt);
  cryptauthv2::ClientMetadata::InvocationReason expected_invocation_reason =
      cryptauthv2::ClientMetadata::PERIODIC;

  EXPECT_EQ(expected_last_enrollment_time,
            enrollment_manager()->GetLastEnrollmentTime());
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentValid());
  EXPECT_EQ(expected_time_to_next_attempt,
            enrollment_manager()->GetTimeToNextAttempt());
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentInProgress());
  EXPECT_FALSE(enrollment_manager()->IsRecoveringFromFailure());

  // First enrollment attempt fails.
  // Note: User does not yet have a GCM registration ID or ClientAppMetadata.
  fake_enrollment_scheduler()->RequestEnrollmentNow();
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      1 /* expected_num_enrollment_started_notifications */);
  fake_gcm_manager()->CompleteRegistration(cryptauthv2::kTestGcmRegistrationId);
  std::move(
      fake_client_app_metadata_provider()->metadata_requests()[0].callback)
      .Run(cryptauthv2::GetClientAppMetadataForTest());
  EXPECT_EQ(cryptauthv2::BuildClientMetadata(0 /* retry_count */,
                                             expected_invocation_reason)
                .SerializeAsString(),
            fake_enroller()->client_metadata()->SerializeAsString());

  CryptAuthEnrollmentResult expected_enrollment_result(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      base::nullopt /* client_directive */);
  fake_enroller()->FinishAttempt(expected_enrollment_result);
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentInProgress());
  VerifyEnrollmentResult(expected_enrollment_result);

  // The initial invocation reason is persisted for reuse in subsequent failure
  // recovery attempts.
  VerifyPersistedFailureRecoveryInvocationReason(
      cryptauthv2::ClientMetadata::PERIODIC);

  // Second (successful) enrollment attempt bypasses GCM registration and
  // ClientAppMetadata fetch because they were performed during the failed
  // attempt.
  fake_enrollment_scheduler()->RequestEnrollmentNow();
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      2 /* expected_num_enrollment_started_notifications */);
  EXPECT_TRUE(enrollment_manager()->IsRecoveringFromFailure());
  expected_invocation_reason = cryptauthv2::ClientMetadata::PERIODIC;
  EXPECT_EQ(cryptauthv2::kTestGcmRegistrationId,
            fake_gcm_manager()->GetRegistrationId());
  EXPECT_EQ(1u,
            fake_client_app_metadata_provider()->metadata_requests().size());
  EXPECT_EQ(
      cryptauthv2::BuildClientMetadata(1 /* retry_count */,
                                       expected_invocation_reason)
          .SerializeAsString(),
      fake_enroller(1u /* index */)->client_metadata()->SerializeAsString());

  expected_enrollment_result = CryptAuthEnrollmentResult(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNoNewKeysNeeded,
      cryptauthv2::GetClientDirectiveForTest());
  fake_enroller(1u /* index */)->FinishAttempt(expected_enrollment_result);
  VerifyEnrollmentResult(expected_enrollment_result);

  // The persisted invocation reason to be used for failure recovery is cleared
  // after a successful enrollment attempt.
  VerifyPersistedFailureRecoveryInvocationReason(
      cryptauthv2::ClientMetadata::INVOCATION_REASON_UNSPECIFIED);

  EXPECT_EQ(test_clock()->Now(), enrollment_manager()->GetLastEnrollmentTime());
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentValid());
  EXPECT_EQ(kFakeRefreshPeriod, enrollment_manager()->GetTimeToNextAttempt());
  EXPECT_FALSE(enrollment_manager()->IsRecoveringFromFailure());
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       EnrollmentTriggeredByGcmMessage) {
  CreateEnrollmentManager();
  enrollment_manager()->Start();

  fake_gcm_manager()->PushReenrollMessage();
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       V1UserKeyPairAddedToRegistryOnConstruction) {
  AddV1UserKeyPairToV1Prefs(kFakeV1PublicKey, kFakeV1PrivateKey);
  CryptAuthKey expected_user_key_pair_v1(
      kFakeV1PublicKey, kFakeV1PrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);

  EXPECT_FALSE(
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair));

  CreateEnrollmentManager();

  EXPECT_EQ(
      expected_user_key_pair_v1,
      *key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair));
  EXPECT_EQ(expected_user_key_pair_v1.public_key(),
            enrollment_manager()->GetUserPublicKey());
  EXPECT_EQ(expected_user_key_pair_v1.private_key(),
            enrollment_manager()->GetUserPrivateKey());
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       V1UserKeyPairOverwritesV2UserKeyPairOnConstruction) {
  AddV1UserKeyPairToV1Prefs(kFakeV1PublicKey, kFakeV1PrivateKey);
  CryptAuthKey expected_user_key_pair_v1(
      kFakeV1PublicKey, kFakeV1PrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);

  // Add v2 user key pair to registry.
  CryptAuthKey user_key_pair_v2(
      kFakeV2PublicKey, kFakeV2PrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);
  key_registry()->AddEnrolledKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                                 user_key_pair_v2);
  EXPECT_EQ(user_key_pair_v2, *key_registry()->GetActiveKey(
                                  CryptAuthKeyBundle::Name::kUserKeyPair));

  // A legacy v1 user key pair should overwrite any existing v2 user key pair
  // when the enrollment manager is constructed. This can happen in the
  // following rollback-->rollforward scenario:
  //   - User never enrolled with v1
  //   - User enrolls with v2, storing v2 user key pair locally in key registry
  //     and in CryptAuth backend database
  //   - Chromium rolls back to v1 enrollment flow
  //   - User enrolls with v1, storing the v1 user key pair in prefs (not key
  //     registry) and overwriting the v2 key in the CryptAuth backend database
  //   - Chromium rolls forward to v2 enrollment flow, with different user key
  //     pairs stored in v1 prefs and the v2 key registry.
  CreateEnrollmentManager();

  EXPECT_EQ(
      expected_user_key_pair_v1,
      *key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair));
  EXPECT_EQ(expected_user_key_pair_v1.public_key(),
            enrollment_manager()->GetUserPublicKey());
  EXPECT_EQ(expected_user_key_pair_v1.private_key(),
            enrollment_manager()->GetUserPrivateKey());
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest, GetUserKeyPair) {
  CreateEnrollmentManager();
  EXPECT_TRUE(enrollment_manager()->GetUserPublicKey().empty());
  EXPECT_TRUE(enrollment_manager()->GetUserPrivateKey().empty());

  key_registry()->AddEnrolledKey(
      CryptAuthKeyBundle::Name::kUserKeyPair,
      CryptAuthKey(kFakeV2PublicKey, kFakeV2PrivateKey,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256,
                   kCryptAuthFixedUserKeyPairHandle));
  EXPECT_EQ(kFakeV2PublicKey, enrollment_manager()->GetUserPublicKey());
  EXPECT_EQ(kFakeV2PrivateKey, enrollment_manager()->GetUserPrivateKey());
}

}  // namespace device_sync

}  // namespace chromeos
