// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_BUILDER_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_BUILDER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/proto/cloud/chrome_extension_policy.pb.h"
#include "chrome/browser/policy/proto/cloud/device_management_local.pb.h"
#include "crypto/rsa_private_key.h"
#include "policy/proto/cloud_policy.pb.h"

namespace policy {

// A helper class for testing that provides a straightforward interface for
// constructing policy blobs for use in testing. NB: This uses fake data and
// hard-coded signing keys by default, so should not be used in production code.
class PolicyBuilder {
 public:
  // Constants used as dummy data for filling the PolicyData protobuf.
  static const char kFakeDeviceId[];
  static const char kFakeDomain[];
  static const char kFakeMachineName[];
  static const char kFakePolicyType[];
  static const int kFakePublicKeyVersion;
  static const int64 kFakeTimestamp;
  static const char kFakeToken[];
  static const char kFakeUsername[];
  static const char kFakeServiceAccountIdentity[];

  // Creates a policy builder. The builder will have all PolicyData fields
  // initialized to dummy values and use the test signing keys.
  PolicyBuilder();
  virtual ~PolicyBuilder();

  // Use this member to access the PolicyData protobuf.
  enterprise_management::PolicyData& policy_data() {
    if (!policy_data_.get())
      policy_data_.reset(new enterprise_management::PolicyData());
    return *policy_data_;
  }
  void clear_policy_data() {
    policy_data_.reset();
  }

  enterprise_management::PolicyFetchResponse& policy() {
    return policy_;
  }

  scoped_ptr<crypto::RSAPrivateKey> GetSigningKey();
  void SetSigningKey(const crypto::RSAPrivateKey& key);
  void SetDefaultSigningKey();
  void UnsetSigningKey();

  scoped_ptr<crypto::RSAPrivateKey> GetNewSigningKey();
  void SetDefaultNewSigningKey();
  void UnsetNewSigningKey();

  // Assembles the policy components. The resulting policy protobuf is available
  // through policy() after this call.
  virtual void Build();

  // Returns a copy of policy().
  scoped_ptr<enterprise_management::PolicyFetchResponse> GetCopy();

  // Returns a binary policy blob, i.e. an encoded PolicyFetchResponse.
  std::string GetBlob();

  // These return hard-coded testing keys. Don't use in production!
  static scoped_ptr<crypto::RSAPrivateKey> CreateTestSigningKey();
  static scoped_ptr<crypto::RSAPrivateKey> CreateTestOtherSigningKey();

 private:
  // Produces |key|'s signature over |data| and stores it in |signature|.
  void SignData(const std::string& data,
                crypto::RSAPrivateKey* key,
                std::string* signature);

  enterprise_management::PolicyFetchResponse policy_;
  scoped_ptr<enterprise_management::PolicyData> policy_data_;
  std::string payload_data_;

  // The keys cannot be stored in NSS. Temporary keys are not guaranteed to
  // remain in the database. Persistent keys require a persistent database,
  // which would coincide with the user's database. However, these keys are used
  // for signing the policy and don't have to coincide with the user's known
  // keys. Instead, we store the private keys as raw bytes. Where needed, a
  // temporary RSAPrivateKey is created.
  std::vector<uint8> raw_signing_key_;
  std::vector<uint8> raw_new_signing_key_;

  DISALLOW_COPY_AND_ASSIGN(PolicyBuilder);
};

// Type-parameterized PolicyBuilder extension that allows for building policy
// blobs carrying protobuf payloads.
template<typename PayloadProto>
class TypedPolicyBuilder : public PolicyBuilder {
 public:
  TypedPolicyBuilder();
  virtual ~TypedPolicyBuilder() {}

  // Returns a reference to the payload protobuf being built.
  PayloadProto& payload() {
    if (!payload_.get())
      payload_.reset(new PayloadProto());
    return *payload_;
  }
  void clear_payload() {
    payload_.reset();
  }

  // PolicyBuilder:
  virtual void Build() OVERRIDE {
    if (payload_.get())
      CHECK(payload_->SerializeToString(policy_data().mutable_policy_value()));

    PolicyBuilder::Build();
  }

 private:
  scoped_ptr<PayloadProto> payload_;

  DISALLOW_COPY_AND_ASSIGN(TypedPolicyBuilder);
};

typedef TypedPolicyBuilder<enterprise_management::CloudPolicySettings>
    UserPolicyBuilder;
typedef TypedPolicyBuilder<enterprise_management::ExternalPolicyData>
    ComponentPolicyBuilder;

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_BUILDER_H_
