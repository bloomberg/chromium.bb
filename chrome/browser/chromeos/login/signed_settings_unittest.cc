// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/dbus/mock_dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/mock_session_manager_client.h"
#include "chrome/browser/chromeos/login/mock_owner_key_utils.h"
#include "chrome/browser/chromeos/login/mock_ownership_service.h"
#include "chrome/browser/chromeos/login/owner_manager_unittest.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::A;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::WithArg;
using ::testing::_;
using content::BrowserThread;
using google::protobuf::RepeatedPtrField;

namespace em = enterprise_management;
namespace chromeos {

namespace {
template <class T>
class DummyDelegate : public SignedSettings::Delegate<T> {
 public:
  explicit DummyDelegate(T to_expect)
      : expect_success_(false),
        expected_failure_(SignedSettings::SUCCESS),
        expected_(to_expect),
        run_(false) {}
  virtual ~DummyDelegate() { EXPECT_TRUE(run_); }
  virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                     T value) {
    run_ = true;
    if (expect_success_)
      compare_expected(value);
    EXPECT_EQ(expected_failure_, code);
  }
  virtual void expect_success() {
    expect_success_ = true;
    expected_failure_ = SignedSettings::SUCCESS;
  }
  virtual void expect_failure(SignedSettings::ReturnCode code) {
    expect_success_ = false;
    expected_failure_ = code;
  }

 protected:
  bool expect_success_;
  SignedSettings::ReturnCode expected_failure_;
  T expected_;
  bool run_;
  virtual void compare_expected(T to_compare) = 0;
};

template <class T>
class NormalDelegate : public DummyDelegate<T> {
 public:
  explicit NormalDelegate(T to_expect) : DummyDelegate<T>(to_expect) {}
  virtual ~NormalDelegate() {}
 protected:
  virtual void compare_expected(T to_compare) {
    // without this-> this won't build.
    EXPECT_EQ(this->expected_, to_compare);
  }
};

class ProtoDelegate : public DummyDelegate<const em::PolicyFetchResponse&> {
 public:
  explicit ProtoDelegate(const em::PolicyFetchResponse& e)
      : DummyDelegate<const em::PolicyFetchResponse&>(e) {
  }
  virtual ~ProtoDelegate() {}
 protected:
  virtual void compare_expected(const em::PolicyFetchResponse& to_compare) {
    std::string ex_string, comp_string;
    EXPECT_TRUE(expected_.SerializeToString(&ex_string));
    EXPECT_TRUE(to_compare.SerializeToString(&comp_string));
    EXPECT_EQ(ex_string, comp_string);
  }
};

}  // anonymous namespace

class SignedSettingsTest : public testing::Test {
 public:
  SignedSettingsTest()
      : fake_prop_(kAccountsPrefAllowGuest),
        fake_signature_("false"),
        fake_value_(false),
        fake_value_signature_(
            fake_signature_.c_str(),
            fake_signature_.c_str() + fake_signature_.length()),
        message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE),
        mock_(new MockKeyUtils),
        injector_(mock_) /* injector_ takes ownership of mock_ */,
        mock_dbus_thread_manager_(new MockDBusThreadManager) {
  }

  virtual ~SignedSettingsTest() {}

  virtual void SetUp() {
    file_thread_.Start();
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager_);
  }

  virtual void TearDown() {
    OwnerKeyUtils::set_factory(NULL);
    DBusThreadManager::Shutdown();
  }

  void mock_service(SignedSettings* s, MockOwnershipService* m) {
    s->set_service(m);
  }

  void FailingStorePolicyOp(const OwnerManager::KeyOpCode return_code) {
    NormalDelegate<bool> d(false);
    d.expect_failure(SignedSettings::MapKeyOpCode(return_code));

    em::PolicyFetchResponse fake_policy;
    fake_policy.set_policy_data(fake_prop_);
    std::string serialized;
    ASSERT_TRUE(fake_policy.SerializeToString(&serialized));

    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateStorePolicyOp(&fake_policy, &d));

    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartSigningAttempt(StrEq(fake_prop_), _))
        .Times(1);

    s->Execute();
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
    message_loop_.RunAllPending();
  }

  em::PolicyData BuildPolicyData(std::vector<std::string> whitelist) {
    em::PolicyData to_return;
    em::ChromeDeviceSettingsProto pol;
    em::GuestModeEnabledProto* allow = pol.mutable_guest_mode_enabled();
    allow->set_guest_mode_enabled(false);
    pol.mutable_device_proxy_settings()->set_proxy_mode("direct");

    if (!whitelist.empty()) {
      em::UserWhitelistProto* whitelist_proto = pol.mutable_user_whitelist();
      for (std::vector<std::string>::const_iterator it = whitelist.begin();
           it != whitelist.end();
           ++it) {
        whitelist_proto->add_user_whitelist(*it);
      }
    }

    to_return.set_policy_type(chromeos::kDevicePolicyType);
    to_return.set_policy_value(pol.SerializeAsString());
    return to_return;
  }

  em::PolicyFetchResponse BuildProto(const std::string& data,
                                     const std::string& sig,
                                     std::string* out_serialized) {
    em::PolicyFetchResponse fake_policy;
    if (!data.empty())
      fake_policy.set_policy_data(data);
    if (!sig.empty())
      fake_policy.set_policy_data_signature(sig);
    EXPECT_TRUE(fake_policy.SerializeToString(out_serialized));
    return fake_policy;
  }

  const std::string fake_prop_;
  const std::string fake_signature_;
  const base::FundamentalValue fake_value_;
  const std::vector<uint8> fake_value_signature_;
  MockOwnershipService m_;

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  MockKeyUtils* mock_;
  MockInjector injector_;
  MockDBusThreadManager* mock_dbus_thread_manager_;

  ScopedStubCrosEnabler stub_cros_enabler_;
};

ACTION_P(Retrieve, policy_blob) { arg0.Run(policy_blob); }
ACTION_P(Store, success) { arg1.Run(success); }

TEST_F(SignedSettingsTest, SignAndStorePolicy) {
  NormalDelegate<bool> d(true);
  d.expect_success();

  em::PolicyData in_pol = BuildPolicyData(std::vector<std::string>());
  std::string data_serialized = in_pol.SerializeAsString();
  std::string serialized;
  em::PolicyFetchResponse fake_policy = BuildProto(data_serialized,
                                                   std::string(),
                                                   &serialized);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePolicyOp(&fake_policy, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(StrEq(data_serialized), _))
      .Times(1);
  em::PolicyData out_pol;

  // Ask for signature over unsigned policy.
  s->Execute();
  message_loop_.RunAllPending();

  // Fake out a successful signing.
  std::string signed_serialized;
  em::PolicyFetchResponse signed_policy = BuildProto(data_serialized,
                                                     fake_signature_,
                                                     &signed_serialized);
  MockSessionManagerClient* client =
      mock_dbus_thread_manager_->mock_session_manager_client();
  EXPECT_CALL(*client, StorePolicy(signed_serialized, _))
      .WillOnce(Store(true))
      .RetiresOnSaturation();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, fake_value_signature_);
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, StoreSignedPolicy) {
  NormalDelegate<bool> d(true);
  d.expect_success();

  em::PolicyData in_pol = BuildPolicyData(std::vector<std::string>());
  std::string serialized = in_pol.SerializeAsString();
  std::string signed_serialized;
  em::PolicyFetchResponse signed_policy = BuildProto(serialized,
                                                     fake_signature_,
                                                     &signed_serialized);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePolicyOp(&signed_policy, &d));
  MockSessionManagerClient* client =
      mock_dbus_thread_manager_->mock_session_manager_client();
  EXPECT_CALL(*client, StorePolicy(signed_serialized, _))
      .WillOnce(Store(true))
      .RetiresOnSaturation();

  mock_service(s.get(), &m_);
  em::PolicyData out_pol;

  s->Execute();
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, StorePolicyNoKey) {
  FailingStorePolicyOp(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, StorePolicyFailed) {
  FailingStorePolicyOp(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, StorePolicyNoPolicyData) {
  NormalDelegate<bool> d(false);
  d.expect_failure(SignedSettings::OPERATION_FAILED);

  std::string serialized;
  em::PolicyFetchResponse fake_policy = BuildProto(std::string(),
                                                   std::string(),
                                                   &serialized);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePolicyOp(&fake_policy, &d));

  s->Execute();
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, RetrievePolicy) {
  em::PolicyData in_pol = BuildPolicyData(std::vector<std::string>());
  std::string serialized = in_pol.SerializeAsString();
  std::string signed_serialized;
  em::PolicyFetchResponse signed_policy = BuildProto(serialized,
                                                     fake_signature_,
                                                     &signed_serialized);
  ProtoDelegate d(signed_policy);
  d.expect_success();
  scoped_refptr<SignedSettings> s(SignedSettings::CreateRetrievePolicyOp(&d));

  MockSessionManagerClient* client =
      mock_dbus_thread_manager_->mock_session_manager_client();
  EXPECT_CALL(*client, RetrievePolicy(_))
      .WillOnce(Retrieve(signed_serialized))
      .RetiresOnSaturation();

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartVerifyAttempt(serialized, fake_value_signature_, _))
      .Times(1);
  em::PolicyData out_pol;

  s->Execute();
  message_loop_.RunAllPending();

  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, RetrieveNullPolicy) {
  em::PolicyFetchResponse policy;
  ProtoDelegate d(policy);
  d.expect_failure(SignedSettings::NOT_FOUND);
  scoped_refptr<SignedSettings> s(SignedSettings::CreateRetrievePolicyOp(&d));

  MockSessionManagerClient* client =
      mock_dbus_thread_manager_->mock_session_manager_client();
  EXPECT_CALL(*client, RetrievePolicy(_))
      .WillOnce(Retrieve(""))
      .RetiresOnSaturation();

  s->Execute();
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, RetrieveEmptyPolicy) {
  std::string serialized;
  em::PolicyFetchResponse policy = BuildProto("", "", &serialized);
  ProtoDelegate d(policy);
  d.expect_failure(SignedSettings::NOT_FOUND);
  scoped_refptr<SignedSettings> s(SignedSettings::CreateRetrievePolicyOp(&d));

  MockSessionManagerClient* client =
      mock_dbus_thread_manager_->mock_session_manager_client();
  EXPECT_CALL(*client, RetrievePolicy(_))
      .WillOnce(Retrieve(""))
      .RetiresOnSaturation();

  s->Execute();
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, RetrieveUnsignedPolicy) {
  std::string serialized;
  em::PolicyFetchResponse policy = BuildProto(fake_prop_,
                                              std::string(),
                                              &serialized);
  ProtoDelegate d(policy);
  d.expect_failure(SignedSettings::BAD_SIGNATURE);
  scoped_refptr<SignedSettings> s(SignedSettings::CreateRetrievePolicyOp(&d));

  MockSessionManagerClient* client =
      mock_dbus_thread_manager_->mock_session_manager_client();
  EXPECT_CALL(*client, RetrievePolicy(_))
      .WillOnce(Retrieve(serialized))
      .RetiresOnSaturation();

  s->Execute();
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, RetrieveMalsignedPolicy) {
  std::string signed_serialized;
  em::PolicyFetchResponse signed_policy = BuildProto(fake_prop_,
                                                     fake_signature_,
                                                     &signed_serialized);
  ProtoDelegate d(signed_policy);
  d.expect_failure(SignedSettings::BAD_SIGNATURE);
  scoped_refptr<SignedSettings> s(SignedSettings::CreateRetrievePolicyOp(&d));

  MockSessionManagerClient* client =
      mock_dbus_thread_manager_->mock_session_manager_client();
  EXPECT_CALL(*client, RetrievePolicy(_))
      .WillOnce(Retrieve(signed_serialized))
      .RetiresOnSaturation();

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartVerifyAttempt(fake_prop_, fake_value_signature_, _))
      .Times(1);

  s->Execute();
  message_loop_.RunAllPending();

  s->OnKeyOpComplete(OwnerManager::OPERATION_FAILED, std::vector<uint8>());
  message_loop_.RunAllPending();
}

}  // namespace chromeos
