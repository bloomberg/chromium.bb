// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/proto/cryptauth_v2_test_util.h"

#include "base/no_destructor.h"
#include "chromeos/services/device_sync/public/cpp/gcm_constants.h"

namespace cryptauthv2 {

// Attributes of test ClientDirective.
const int32_t kTestClientDirectiveRetryAttempts = 3;
const int64_t kTestClientDirectiveCheckinDelayMillis = 2592000000;  // 30 days
const int64_t kTestClientDirectivePolicyReferenceVersion = 2;
const int64_t kTestClientDirectiveRetryPeriodMillis = 43200000;  // 12 hours
const int64_t kTestClientDirectiveCreateTimeMillis = 1566073800000;
const char kTestClientDirectiveInvokeNextKeyName[] =
    "client_directive_invoke_next_key_name";
const char kTestClientDirectivePolicyReferenceName[] =
    "client_directive_policy_reference_name";
const TargetService kTestClientDirectiveInvokeNextService =
    TargetService::DEVICE_SYNC;

// Attributes of test ClientAppMetadata.
const char kTestGcmRegistrationId[] = "gcm_registraion_id";
const char kTestInstanceId[] = "instance_id";
const char kTestInstanceIdToken[] = "instance_id_token";
const char kTestLongDeviceId[] = "long_device_id";

ClientMetadata BuildClientMetadata(
    int32_t retry_count,
    const ClientMetadata::InvocationReason& invocation_reason) {
  ClientMetadata client_metadata;
  client_metadata.set_retry_count(retry_count);
  client_metadata.set_invocation_reason(invocation_reason);

  return client_metadata;
}

PolicyReference BuildPolicyReference(const std::string& name, int64_t version) {
  PolicyReference policy_reference;
  policy_reference.set_name(name);
  policy_reference.set_version(version);

  return policy_reference;
}

KeyDirective BuildKeyDirective(const PolicyReference& policy_reference,
                               int64_t enroll_time_millis) {
  KeyDirective key_directive;
  key_directive.mutable_policy_reference()->CopyFrom(policy_reference);
  key_directive.set_enroll_time_millis(enroll_time_millis);

  return key_directive;
}

const ClientAppMetadata& GetClientAppMetadataForTest() {
  static const base::NoDestructor<ClientAppMetadata> metadata([] {
    ApplicationSpecificMetadata app_specific_metadata;
    app_specific_metadata.set_gcm_registration_id(kTestGcmRegistrationId);
    app_specific_metadata.set_device_software_package(
        chromeos::device_sync::kCryptAuthGcmAppId);

    BetterTogetherFeatureMetadata beto_metadata;
    beto_metadata.add_supported_features(
        BetterTogetherFeatureMetadata::BETTER_TOGETHER_CLIENT);
    beto_metadata.add_supported_features(
        BetterTogetherFeatureMetadata::SMS_CONNECT_CLIENT);

    FeatureMetadata feature_metadata;
    feature_metadata.set_feature_type(FeatureMetadata::BETTER_TOGETHER);
    feature_metadata.set_metadata(beto_metadata.SerializeAsString());

    ClientAppMetadata metadata;
    metadata.add_application_specific_metadata()->CopyFrom(
        app_specific_metadata);
    metadata.set_instance_id(kTestInstanceId);
    metadata.set_instance_id_token(kTestInstanceIdToken);
    metadata.set_long_device_id(kTestLongDeviceId);
    metadata.add_feature_metadata()->CopyFrom(feature_metadata);

    return metadata;
  }());

  return *metadata;
}

const ClientDirective& GetClientDirectiveForTest() {
  static const base::NoDestructor<ClientDirective> client_directive([] {
    InvokeNext invoke_next;
    invoke_next.set_service(kTestClientDirectiveInvokeNextService);
    invoke_next.set_key_name(kTestClientDirectiveInvokeNextKeyName);

    ClientDirective client_directive;
    client_directive.mutable_policy_reference()->CopyFrom(
        BuildPolicyReference(kTestClientDirectivePolicyReferenceName,
                             kTestClientDirectivePolicyReferenceVersion));
    client_directive.set_checkin_delay_millis(
        kTestClientDirectiveCheckinDelayMillis);
    client_directive.set_retry_attempts(kTestClientDirectiveRetryAttempts);
    client_directive.set_retry_period_millis(
        kTestClientDirectiveRetryPeriodMillis);
    client_directive.set_create_time_millis(
        kTestClientDirectiveCreateTimeMillis);
    client_directive.add_invoke_next()->CopyFrom(invoke_next);

    return client_directive;
  }());
  return *client_directive;
}

}  // namespace cryptauthv2
