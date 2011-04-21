// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/mock_owner_key_utils.h"
#include "chrome/browser/chromeos/login/mock_ownership_service.h"
#include "chrome/browser/chromeos/login/owner_manager_unittest.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/test/thread_test_helper.h"
#include "content/browser/browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::A;
using ::testing::AnyNumber;
using ::testing::InvokeArgument;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::WithArg;
using ::testing::_;
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
    EXPECT_EQ(this->expected_, to_compare);  // without this-> this won't build.
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

class SignedSettingsTest : public ::testing::Test {
 public:
  SignedSettingsTest()
      : fake_email_("fakey@example.com"),
        fake_domain_("*@example.com"),
        fake_prop_(kAccountsPrefAllowGuest),
        fake_value_("false"),
        message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE),
        mock_(new MockKeyUtils),
        injector_(mock_) /* injector_ takes ownership of mock_ */ {
  }

  virtual ~SignedSettingsTest() {}

  virtual void SetUp() {
    file_thread_.Start();
  }

  virtual void TearDown() {
    OwnerKeyUtils::set_factory(NULL);
  }

  void mock_service(SignedSettings* s, MockOwnershipService* m) {
    s->set_service(m);
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

    to_return.set_policy_type(SignedSettings::kDevicePolicyType);
    to_return.set_policy_value(pol.SerializeAsString());
    return to_return;
  }

  void SetAllowNewUsers(bool desired, em::PolicyData* poldata) {
    em::ChromeDeviceSettingsProto pol;
    pol.ParseFromString(poldata->policy_value());
    em::AllowNewUsersProto* allow = pol.mutable_allow_new_users();
    allow->set_allow_new_users(desired);
    poldata->set_policy_value(pol.SerializeAsString());
  }

  bool CheckWhitelist(const std::string& email, const em::PolicyData& poldata) {
    if (!poldata.has_policy_value())
      return false;
    em::ChromeDeviceSettingsProto pol;
    pol.ParseFromString(poldata.policy_value());
    if (!pol.has_user_whitelist())
      return false;

    const RepeatedPtrField<std::string>& whitelist =
        pol.user_whitelist().user_whitelist();
    for (RepeatedPtrField<std::string>::const_iterator it = whitelist.begin();
         it != whitelist.end();
         ++it) {
      if (email == *it)
        return true;
    }
    return false;
  }

  void ExpectWhitelistOp(SignedSettings* s,
                         em::PolicyData* fake_pol,
                         em::PolicyData* out_pol) {
    mock_service(s, &m_);
    EXPECT_CALL(m_, StartSigningAttempt(_, _))
        .Times(1);
    EXPECT_CALL(m_, has_cached_policy())
        .WillOnce(Return(true));
    EXPECT_CALL(m_, cached_policy())
        .WillOnce(ReturnRef(*fake_pol));
    EXPECT_CALL(m_, set_cached_policy(A<const em::PolicyData&>()))
        .WillOnce(SaveArg<0>(out_pol));
  }

  void FailingStorePropertyOp(const OwnerManager::KeyOpCode return_code) {
    NormalDelegate<bool> d(false);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateStorePropertyOp(fake_prop_, fake_value_, &d));
    d.expect_failure(SignedSettings::MapKeyOpCode(return_code));

    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartSigningAttempt(_, _))
        .Times(1);
    EXPECT_CALL(m_, GetStatus(_))
        .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));
    EXPECT_CALL(m_, has_cached_policy())
        .WillOnce(Return(true));
    em::PolicyData fake_pol;
    EXPECT_CALL(m_, cached_policy())
        .WillOnce(ReturnRef(fake_pol));

    s->Execute();
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
    message_loop_.RunAllPending();
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

  MockLoginLibrary* MockLoginLib() {
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();

    // Mocks, ownership transferred to CrosLibrary class on creation.
    MockLoginLibrary* mock_library;
    MockLibraryLoader* loader;

    loader = new MockLibraryLoader();
    ON_CALL(*loader, Load(_))
        .WillByDefault(Return(true));
    EXPECT_CALL(*loader, Load(_))
        .Times(AnyNumber());

    test_api->SetLibraryLoader(loader, true);

    mock_library = new MockLoginLibrary();
    test_api->SetLoginLibrary(mock_library, true);
    return mock_library;
  }

  void UnMockLoginLib() {
    // Prevent bogus gMock leak check from firing.
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();
    test_api->SetLibraryLoader(NULL, false);
    test_api->SetLoginLibrary(NULL, false);
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

  void DoRetrieveProperty(const std::string& name,
                          const std::string& value,
                          em::PolicyData* fake_pol) {
    NormalDelegate<std::string> d(value);
    d.expect_success();
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateRetrievePropertyOp(name, &d));
    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, GetStatus(_))
        .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));
    EXPECT_CALL(m_, has_cached_policy())
        .WillOnce(Return(true));

    EXPECT_CALL(m_, cached_policy())
        .WillOnce(ReturnRef(*fake_pol));

    s->Execute();
    message_loop_.RunAllPending();
  }

  const std::string fake_email_;
  const std::string fake_domain_;
  const std::string fake_prop_;
  const std::string fake_value_;
  MockOwnershipService m_;

  ScopedTempDir tmpdir_;
  FilePath tmpfile_;

  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  std::vector<uint8> fake_public_key_;
  scoped_ptr<crypto::RSAPrivateKey> fake_private_key_;

  MockKeyUtils* mock_;
  MockInjector injector_;

  ScopedStubCrosEnabler stub_cros_enabler_;
};

TEST_F(SignedSettingsTest, CheckWhitelist) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateCheckWhitelistOp(fake_email_, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, has_cached_policy())
      .WillOnce(Return(true));

  std::vector<std::string> whitelist(1, fake_email_);
  whitelist.push_back(fake_email_ + "m");
  em::PolicyData fake_pol = BuildPolicyData(whitelist);
  EXPECT_CALL(m_, cached_policy())
      .WillOnce(ReturnRef(fake_pol));

  s->Execute();
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, CheckWhitelistWildcards) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateCheckWhitelistOp(fake_email_, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, has_cached_policy())
      .WillOnce(Return(true));

  std::vector<std::string> whitelist(1, fake_domain_);
  whitelist.push_back(fake_email_ + "m");
  em::PolicyData fake_pol = BuildPolicyData(whitelist);
  EXPECT_CALL(m_, cached_policy())
      .WillOnce(ReturnRef(fake_pol))
      .WillOnce(ReturnRef(fake_pol));

  s->Execute();
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, CheckWhitelistNotFound) {
  NormalDelegate<bool> d(true);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateCheckWhitelistOp(fake_email_, &d));
  d.expect_failure(SignedSettings::NOT_FOUND);

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, has_cached_policy())
      .WillOnce(Return(true));

  std::vector<std::string> whitelist(1, fake_email_ + "m");
  em::PolicyData fake_pol = BuildPolicyData(whitelist);
  EXPECT_CALL(m_, cached_policy())
      .WillOnce(ReturnRef(fake_pol))
      .WillOnce(ReturnRef(fake_pol));

  s->Execute();
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, Whitelist) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateWhitelistOp(fake_email_, true, &d));
  em::PolicyData in_pol = BuildPolicyData(std::vector<std::string>());
  em::PolicyData out_pol;
  ExpectWhitelistOp(s.get(), &in_pol, &out_pol);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();

  ASSERT_TRUE(CheckWhitelist(fake_email_, out_pol));
}

TEST_F(SignedSettingsTest, AddToExistingWhitelist) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateWhitelistOp(fake_email_, true, &d));
  em::PolicyData in_pol =
      BuildPolicyData(std::vector<std::string>(1, fake_domain_));
  em::PolicyData out_pol;
  ExpectWhitelistOp(s.get(), &in_pol, &out_pol);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();

  ASSERT_TRUE(CheckWhitelist(fake_email_, out_pol));
}

TEST_F(SignedSettingsTest, Unwhitelist) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateWhitelistOp(fake_email_, false, &d));
  em::PolicyData in_pol =
      BuildPolicyData(std::vector<std::string>(1, fake_email_));
  em::PolicyData out_pol;
  ExpectWhitelistOp(s.get(), &in_pol, &out_pol);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();

  ASSERT_FALSE(CheckWhitelist(fake_email_, out_pol));
}

TEST_F(SignedSettingsTest, RemoveFromExistingWhitelist) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateWhitelistOp(fake_email_, false, &d));
  std::vector<std::string> whitelist(1, fake_domain_);
  whitelist.push_back(fake_email_);
  whitelist.push_back(fake_email_ + "m");
  em::PolicyData in_pol = BuildPolicyData(whitelist);
  em::PolicyData out_pol;
  ExpectWhitelistOp(s.get(), &in_pol, &out_pol);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();

  ASSERT_FALSE(CheckWhitelist(fake_email_, out_pol));
}

TEST_F(SignedSettingsTest, StoreProperty) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePropertyOp(fake_prop_, fake_value_, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(_, _))
      .Times(1);
  EXPECT_CALL(m_, GetStatus(_))
      .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));
  EXPECT_CALL(m_, has_cached_policy())
      .WillOnce(Return(true));
  em::PolicyData in_pol =
      BuildPolicyData(std::vector<std::string>(1, fake_email_));
  EXPECT_CALL(m_, cached_policy())
      .WillOnce(ReturnRef(in_pol));
  em::PolicyData out_pol;
  EXPECT_CALL(m_, set_cached_policy(A<const em::PolicyData&>()))
      .WillOnce(SaveArg<0>(&out_pol));

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();

  ASSERT_TRUE(out_pol.has_policy_value());
  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(out_pol.policy_value());
  ASSERT_TRUE(pol.has_guest_mode_enabled());
  ASSERT_TRUE(pol.guest_mode_enabled().has_guest_mode_enabled());
  ASSERT_FALSE(pol.guest_mode_enabled().guest_mode_enabled());
}

TEST_F(SignedSettingsTest, StorePropertyNoKey) {
  FailingStorePropertyOp(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, StorePropertyFailed) {
  FailingStorePropertyOp(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, RetrieveProperty) {
  em::PolicyData fake_pol = BuildPolicyData(std::vector<std::string>());
  DoRetrieveProperty(fake_prop_, fake_value_, &fake_pol);
}

TEST_F(SignedSettingsTest, RetrieveOwnerProperty) {
  em::PolicyData fake_pol = BuildPolicyData(std::vector<std::string>());
  fake_pol.set_username(fake_email_);
  DoRetrieveProperty(kDeviceOwner, fake_email_, &fake_pol);
}

TEST_F(SignedSettingsTest, ExplicitlyAllowNewUsers) {
  em::PolicyData fake_pol = BuildPolicyData(std::vector<std::string>());
  SetAllowNewUsers(true, &fake_pol);
  DoRetrieveProperty(kAccountsPrefAllowNewUser, "true", &fake_pol);
}

TEST_F(SignedSettingsTest, ExplicitlyDisallowNewUsers) {
  std::vector<std::string> whitelist(1, fake_email_ + "m");
  em::PolicyData fake_pol = BuildPolicyData(whitelist);
  SetAllowNewUsers(false, &fake_pol);
  DoRetrieveProperty(kAccountsPrefAllowNewUser, "false", &fake_pol);
}

TEST_F(SignedSettingsTest, ImplicitlyDisallowNewUsers) {
  std::vector<std::string> whitelist(1, fake_email_ + "m");
  em::PolicyData fake_pol = BuildPolicyData(whitelist);
  DoRetrieveProperty(kAccountsPrefAllowNewUser, "false", &fake_pol);
}

TEST_F(SignedSettingsTest, AccidentallyDisallowNewUsers) {
  em::PolicyData fake_pol = BuildPolicyData(std::vector<std::string>());
  SetAllowNewUsers(false, &fake_pol);
  DoRetrieveProperty(kAccountsPrefAllowNewUser, "true", &fake_pol);
}

TEST_F(SignedSettingsTest, RetrievePropertyNotFound) {
  NormalDelegate<std::string> d(fake_value_);
  d.expect_failure(SignedSettings::NOT_FOUND);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateRetrievePropertyOp("unknown_prop", &d));
  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, GetStatus(_))
      .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));
  EXPECT_CALL(m_, has_cached_policy())
      .WillOnce(Return(true));

  em::PolicyData fake_pol = BuildPolicyData(std::vector<std::string>());
  EXPECT_CALL(m_, cached_policy())
      .WillOnce(ReturnRef(fake_pol));

  s->Execute();
  message_loop_.RunAllPending();
}

ACTION_P(Retrieve, s) { (*arg0)((void*)arg1, s.c_str(), s.length()); }
ACTION_P(FinishKeyOp, s) { arg2->OnKeyOpComplete(OwnerManager::SUCCESS, s); }

TEST_F(SignedSettingsTest, RetrievePolicyToRetrieveProperty) {
  NormalDelegate<std::string> d(fake_value_);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateRetrievePropertyOp(fake_prop_, &d));

  em::PolicyData fake_pol = BuildPolicyData(std::vector<std::string>());
  std::string data = fake_pol.SerializeAsString();
  std::string signed_serialized;
  em::PolicyFetchResponse signed_policy = BuildProto(data,
                                                     fake_value_,
                                                     &signed_serialized);
  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestRetrievePolicy(_, _))
      .WillOnce(Retrieve(signed_serialized))
      .RetiresOnSaturation();

  mock_service(s.get(), &m_);

  EXPECT_CALL(m_, GetStatus(_))
      .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN))
      .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));
  EXPECT_CALL(m_, has_cached_policy())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  em::PolicyData out_pol;
  EXPECT_CALL(m_, set_cached_policy(A<const em::PolicyData&>()))
      .WillOnce(SaveArg<0>(&out_pol));
  EXPECT_CALL(m_, cached_policy())
      .WillOnce(ReturnRef(out_pol));

  std::vector<uint8> fake_sig(fake_value_.c_str(),
                              fake_value_.c_str() + fake_value_.length());
  EXPECT_CALL(m_, StartVerifyAttempt(data, fake_sig, _))
      .WillOnce(FinishKeyOp(fake_sig))
      .RetiresOnSaturation();

  s->Execute();
  message_loop_.RunAllPending();
  UnMockLoginLib();
}

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
  EXPECT_CALL(m_, set_cached_policy(A<const em::PolicyData&>()))
      .WillOnce(SaveArg<0>(&out_pol));

  // Ask for signature over unsigned policy.
  s->Execute();
  message_loop_.RunAllPending();

  // Fake out a successful signing.
  std::string signed_serialized;
  em::PolicyFetchResponse signed_policy = BuildProto(data_serialized,
                                                     fake_value_,
                                                     &signed_serialized);
  std::vector<uint8> fake_sig(fake_value_.c_str(),
                              fake_value_.c_str() + fake_value_.length());

  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestStorePolicy(StrEq(signed_serialized), _, s.get()))
      .WillOnce(InvokeArgument<1>(static_cast<void*>(s.get()), true))
      .RetiresOnSaturation();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, fake_sig);
  message_loop_.RunAllPending();
  UnMockLoginLib();
}

TEST_F(SignedSettingsTest, StoreSignedPolicy) {
  NormalDelegate<bool> d(true);
  d.expect_success();

  em::PolicyData in_pol = BuildPolicyData(std::vector<std::string>());
  std::string serialized = in_pol.SerializeAsString();
  std::string signed_serialized;
  em::PolicyFetchResponse signed_policy = BuildProto(serialized,
                                                     fake_value_,
                                                     &signed_serialized);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePolicyOp(&signed_policy, &d));
  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestStorePolicy(StrEq(signed_serialized), _, s.get()))
      .WillOnce(InvokeArgument<1>(static_cast<void*>(s.get()), true))
      .RetiresOnSaturation();

  mock_service(s.get(), &m_);
  em::PolicyData out_pol;
  EXPECT_CALL(m_, set_cached_policy(A<const em::PolicyData&>()))
      .WillOnce(SaveArg<0>(&out_pol));

  s->Execute();
  message_loop_.RunAllPending();
  UnMockLoginLib();
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
                                                     fake_value_,
                                                     &signed_serialized);
  ProtoDelegate d(signed_policy);
  d.expect_success();
  scoped_refptr<SignedSettings> s(SignedSettings::CreateRetrievePolicyOp(&d));

  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestRetrievePolicy(_, s.get()))
      .WillOnce(InvokeArgument<0>(static_cast<void*>(s.get()),
                                  signed_serialized.c_str(),
                                  signed_serialized.length()))
      .RetiresOnSaturation();

  mock_service(s.get(), &m_);
  std::vector<uint8> fake_sig(fake_value_.c_str(),
                              fake_value_.c_str() + fake_value_.length());
  EXPECT_CALL(m_, StartVerifyAttempt(serialized, fake_sig, _))
      .Times(1);
  em::PolicyData out_pol;
  EXPECT_CALL(m_, set_cached_policy(A<const em::PolicyData&>()))
      .WillOnce(SaveArg<0>(&out_pol));

  s->Execute();
  message_loop_.RunAllPending();
  UnMockLoginLib();

  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, RetrieveNullPolicy) {
  em::PolicyFetchResponse policy;
  ProtoDelegate d(policy);
  d.expect_failure(SignedSettings::NOT_FOUND);
  scoped_refptr<SignedSettings> s(SignedSettings::CreateRetrievePolicyOp(&d));

  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestRetrievePolicy(_, s.get()))
      .WillOnce(InvokeArgument<0>(static_cast<void*>(s.get()),
                                  static_cast<const char*>(NULL),
                                  0))
      .RetiresOnSaturation();

  s->Execute();
  message_loop_.RunAllPending();
  UnMockLoginLib();
}

TEST_F(SignedSettingsTest, RetrieveEmptyPolicy) {
  std::string serialized;
  em::PolicyFetchResponse policy = BuildProto("", "", &serialized);
  ProtoDelegate d(policy);
  d.expect_failure(SignedSettings::NOT_FOUND);
  scoped_refptr<SignedSettings> s(SignedSettings::CreateRetrievePolicyOp(&d));

  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestRetrievePolicy(_, s.get()))
      .WillOnce(InvokeArgument<0>(static_cast<void*>(s.get()), "", 0))
      .RetiresOnSaturation();

  s->Execute();
  message_loop_.RunAllPending();
  UnMockLoginLib();
}

TEST_F(SignedSettingsTest, RetrieveUnsignedPolicy) {
  std::string serialized;
  em::PolicyFetchResponse policy = BuildProto(fake_prop_,
                                              std::string(),
                                              &serialized);
  ProtoDelegate d(policy);
  d.expect_failure(SignedSettings::BAD_SIGNATURE);
  scoped_refptr<SignedSettings> s(SignedSettings::CreateRetrievePolicyOp(&d));

  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestRetrievePolicy(_, s.get()))
      .WillOnce(InvokeArgument<0>(static_cast<void*>(s.get()),
                                  serialized.c_str(),
                                  serialized.length()))
      .RetiresOnSaturation();

  s->Execute();
  message_loop_.RunAllPending();
  UnMockLoginLib();
}

TEST_F(SignedSettingsTest, RetrieveMalsignedPolicy) {
  std::string signed_serialized;
  em::PolicyFetchResponse signed_policy = BuildProto(fake_prop_,
                                                     fake_value_,
                                                     &signed_serialized);
  ProtoDelegate d(signed_policy);
  d.expect_failure(SignedSettings::BAD_SIGNATURE);
  scoped_refptr<SignedSettings> s(SignedSettings::CreateRetrievePolicyOp(&d));

  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestRetrievePolicy(_, s.get()))
      .WillOnce(InvokeArgument<0>(static_cast<void*>(s.get()),
                                  signed_serialized.c_str(),
                                  signed_serialized.length()))
      .RetiresOnSaturation();

  mock_service(s.get(), &m_);
  std::vector<uint8> fake_sig(fake_value_.c_str(),
                              fake_value_.c_str() + fake_value_.length());
  EXPECT_CALL(m_, StartVerifyAttempt(fake_prop_, fake_sig, _))
      .Times(1);

  s->Execute();
  message_loop_.RunAllPending();
  UnMockLoginLib();

  s->OnKeyOpComplete(OwnerManager::OPERATION_FAILED, std::vector<uint8>());
  message_loop_.RunAllPending();
}

}  // namespace chromeos
