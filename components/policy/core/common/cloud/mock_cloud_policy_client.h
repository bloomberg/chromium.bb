// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_CLIENT_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class MockCloudPolicyClient : public CloudPolicyClient {
 public:
  MockCloudPolicyClient();
  virtual ~MockCloudPolicyClient();

  MOCK_METHOD2(SetupRegistration, void(const std::string&, const std::string&));
  MOCK_METHOD7(
      Register,
      void(enterprise_management::DeviceRegisterRequest::Type type,
           enterprise_management::DeviceRegisterRequest::Flavor flavor,
           enterprise_management::LicenseType::LicenseTypeEnum license_type,
           const std::string&,
           const std::string&,
           const std::string&,
           const std::string&));
  MOCK_METHOD0(FetchPolicy, void(void));
  MOCK_METHOD0(Unregister, void(void));
  MOCK_METHOD2(UploadEnterpriseMachineCertificate,
               void(const std::string&, const StatusCallback&));
  MOCK_METHOD2(UploadEnterpriseEnrollmentCertificate,
               void(const std::string&, const StatusCallback&));
  MOCK_METHOD3(UploadDeviceStatus,
               void(const enterprise_management::DeviceStatusReportRequest*,
                    const enterprise_management::SessionStatusReportRequest*,
                    const StatusCallback&));
  MOCK_METHOD2(UpdateGcmId, void(const std::string&, const StatusCallback&));

  // Sets the DMToken.
  void SetDMToken(const std::string& token);

  // Injects policy.
  void SetPolicy(const std::string& policy_type,
                 const std::string& settings_entity_id,
                 const enterprise_management::PolicyFetchResponse& policy);

  // Inject invalidation version.
  void SetFetchedInvalidationVersion(
      int64_t fetched_invalidation_version);

  // Sets the status field.
  void SetStatus(DeviceManagementStatus status);

  // Make the notification helpers public.
  using CloudPolicyClient::NotifyPolicyFetched;
  using CloudPolicyClient::NotifyRegistrationStateChanged;
  using CloudPolicyClient::NotifyClientError;

  using CloudPolicyClient::dm_token_;
  using CloudPolicyClient::client_id_;
  using CloudPolicyClient::last_policy_timestamp_;
  using CloudPolicyClient::public_key_version_;
  using CloudPolicyClient::public_key_version_valid_;
  using CloudPolicyClient::types_to_fetch_;
  using CloudPolicyClient::invalidation_version_;
  using CloudPolicyClient::invalidation_payload_;
  using CloudPolicyClient::fetched_invalidation_version_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyClient);
};

class MockCloudPolicyClientObserver : public CloudPolicyClient::Observer {
 public:
  MockCloudPolicyClientObserver();
  virtual ~MockCloudPolicyClientObserver();

  MOCK_METHOD1(OnPolicyFetched, void(CloudPolicyClient*));
  MOCK_METHOD1(OnRegistrationStateChanged, void(CloudPolicyClient*));
  MOCK_METHOD1(OnRobotAuthCodesFetched, void(CloudPolicyClient*));
  MOCK_METHOD1(OnClientError, void(CloudPolicyClient*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyClientObserver);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_CLIENT_H_
