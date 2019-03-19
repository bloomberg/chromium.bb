// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_V2_TEST_UTIL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_V2_TEST_UTIL_H_

#include "chromeos/services/device_sync/proto/cryptauth_better_together_feature_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

namespace cryptauthv2 {

// Attributes of test ClientDirective.
extern const int32_t kTestClientDirectiveRetryAttempts;
extern const int64_t kTestClientDirectiveCheckinDelayMillis;
extern const int64_t kTestClientDirectivePolicyReferenceVersion;
extern const int64_t kTestClientDirectiveRetryPeriodMillis;
extern const int64_t kTestClientDirectiveCreateTimeMillis;
extern const char kTestClientDirectiveInvokeNextKeyName[];
extern const char kTestClientDirectivePolicyReferenceName[];
extern const TargetService kTestClientDirectiveInvokeNextService;

// Attributes of test ClientAppMetadata.
extern const char kTestGcmRegistrationId[];
extern const char kTestInstanceId[];
extern const char kTestInstanceIdToken[];
extern const char kTestLongDeviceId[];

ClientMetadata BuildClientMetadata(
    int32_t retry_count,
    const ClientMetadata::InvocationReason& invocation_reason);
PolicyReference BuildPolicyReference(const std::string& name, int64_t version);
KeyDirective BuildKeyDirective(const PolicyReference& policy_reference,
                               int64_t enroll_time_millis);

const ClientAppMetadata& GetClientAppMetadataForTest();
const ClientDirective& GetClientDirectiveForTest();

}  // namespace cryptauthv2

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_V2_TEST_UTIL_H_
