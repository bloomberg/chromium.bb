// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/nss_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/login/mock_owner_key_utils.h"
#include "chrome/browser/chromeos/login/mock_ownership_service.h"
#include "chrome/browser/chromeos/login/owner_manager_unittest.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/test/thread_test_helper.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::InvokeArgument;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

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
        fake_prop_("prop_name"),
        fake_value_("stub"),
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

  void FailingCheckWhitelist(const OwnerManager::KeyOpCode return_code) {
    NormalDelegate<bool> d(false);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateCheckWhitelistOp(fake_email_, &d));
    d.expect_failure(SignedSettings::MapKeyOpCode(return_code));

    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartVerifyAttempt(fake_email_, _, _))
        .Times(1);

    s->Execute();
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  void FailingWhitelistOp(const OwnerManager::KeyOpCode return_code) {
    NormalDelegate<bool> d(false);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateWhitelistOp(fake_email_, true, &d));
    d.expect_failure(SignedSettings::MapKeyOpCode(return_code));

    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartSigningAttempt(fake_email_, _))
        .Times(1);

    s->Execute();
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  void FailingStorePropertyOp(const OwnerManager::KeyOpCode return_code) {
    NormalDelegate<bool> d(false);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateStorePropertyOp(fake_prop_, fake_value_, &d));
    d.expect_failure(SignedSettings::MapKeyOpCode(return_code));
    std::string to_sign = base::StringPrintf("%s=%s",
                                             fake_prop_.c_str(),
                                             fake_value_.c_str());
    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartSigningAttempt(to_sign, _))
        .Times(1);
    EXPECT_CALL(m_, GetStatus(_))
        .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));

    s->Execute();
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  void FailingRetrievePropertyOp(const OwnerManager::KeyOpCode return_code) {
    NormalDelegate<std::string> d(fake_value_);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateRetrievePropertyOp(fake_prop_, &d));
    d.expect_failure(SignedSettings::MapKeyOpCode(return_code));
    std::string to_verify = base::StringPrintf("%s=%s",
                                               fake_prop_.c_str(),
                                               fake_value_.c_str());
    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartVerifyAttempt(to_verify, _, _))
        .Times(1);
    EXPECT_CALL(m_, GetStatus(_))
        .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));

    s->Execute();
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
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
  scoped_ptr<RSAPrivateKey> fake_private_key_;

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
  EXPECT_CALL(m_, StartVerifyAttempt(fake_email_, _, _))
      .Times(1);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
}

TEST_F(SignedSettingsTest, CheckWhitelistWildcards) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateCheckWhitelistOp(fake_domain_, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartVerifyAttempt(fake_domain_, _, _))
      .Times(1);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
}

TEST_F(SignedSettingsTest, CheckWhitelistNotFound) {
  NormalDelegate<bool> d(true);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateCheckWhitelistOp(fake_email_, &d));
  d.expect_failure(SignedSettings::NOT_FOUND);
  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, CheckWhitelist(fake_email_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  EXPECT_CALL(*lib, CheckWhitelist(fake_domain_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  s->Execute();
  UnMockLoginLib();
}

TEST_F(SignedSettingsTest, CheckWhitelistNoKey) {
  FailingCheckWhitelist(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, CheckWhitelistFailed) {
  FailingCheckWhitelist(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, Whitelist) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateWhitelistOp(fake_email_, true, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(fake_email_, _))
      .Times(1);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, Unwhitelist) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateWhitelistOp(fake_email_, false, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(fake_email_, _))
      .Times(1);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, WhitelistNoKey) {
  FailingWhitelistOp(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, WhitelistFailed) {
  FailingWhitelistOp(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, StoreProperty) {
  NormalDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePropertyOp(fake_prop_, fake_value_, &d));
  std::string to_sign = base::StringPrintf("%s=%s",
                                           fake_prop_.c_str(),
                                           fake_value_.c_str());
  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(to_sign, _))
      .Times(1);
  EXPECT_CALL(m_, GetStatus(_))
      .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, StorePropertyNoKey) {
  FailingStorePropertyOp(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, StorePropertyFailed) {
  FailingStorePropertyOp(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, RetrieveProperty) {
  NormalDelegate<std::string> d(fake_value_);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateRetrievePropertyOp(fake_prop_, &d));
  std::string to_verify = base::StringPrintf("%s=%s",
                                             fake_prop_.c_str(),
                                             fake_value_.c_str());
  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartVerifyAttempt(to_verify, _, _))
      .Times(1);
  EXPECT_CALL(m_, GetStatus(_))
      .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
}

TEST_F(SignedSettingsTest, RetrievePropertyNotFound) {
  NormalDelegate<std::string> d(fake_value_);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateRetrievePropertyOp(fake_prop_, &d));
  d.expect_failure(SignedSettings::NOT_FOUND);
  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestRetrieveProperty(fake_prop_, _, _))
      .WillOnce(
          InvokeArgument<1>(static_cast<void*>(s.get()),
                            false,
                            static_cast<chromeos::Property*>(NULL)))
      .RetiresOnSaturation();
  s->Execute();
  UnMockLoginLib();
}

TEST_F(SignedSettingsTest, RetrievePropertyNoKey) {
  FailingRetrievePropertyOp(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, RetrievePropertyFailed) {
  FailingRetrievePropertyOp(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, SignAndStorePolicy) {
  NormalDelegate<bool> d(true);
  d.expect_success();

  std::string serialized;
  em::PolicyFetchResponse fake_policy = BuildProto(fake_prop_,
                                                   std::string(),
                                                   &serialized);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePolicyOp(&fake_policy, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(StrEq(fake_prop_), _))
      .Times(1);

  // Ask for signature over unsigned policy.
  s->Execute();
  message_loop_.RunAllPending();

  // Fake out a successful signing.
  std::string signed_serialized;
  em::PolicyFetchResponse signed_policy = BuildProto(fake_prop_,
                                                     fake_value_,
                                                     &signed_serialized);
  std::vector<uint8> fake_sig(fake_value_.c_str(),
                              fake_value_.c_str() + fake_value_.length());

  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestStorePolicy(StrEq(signed_serialized), _, &d))
      .WillOnce(InvokeArgument<1>(static_cast<void*>(&d), true))
      .RetiresOnSaturation();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, fake_sig);
  UnMockLoginLib();
}

TEST_F(SignedSettingsTest, StoreSignedPolicy) {
  NormalDelegate<bool> d(true);
  d.expect_success();

  std::string serialized;
  em::PolicyFetchResponse fake_policy = BuildProto(fake_prop_,
                                                   fake_value_,
                                                   &serialized);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePolicyOp(&fake_policy, &d));
  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RequestStorePolicy(StrEq(serialized), _, &d))
      .WillOnce(InvokeArgument<1>(static_cast<void*>(&d), true))
      .RetiresOnSaturation();
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
  d.expect_failure(
      SignedSettings::MapKeyOpCode(OwnerManager::OPERATION_FAILED));

  std::string serialized;
  em::PolicyFetchResponse fake_policy = BuildProto(std::string(),
                                                   std::string(),
                                                   &serialized);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePolicyOp(&fake_policy, &d));

  s->Execute();
}

TEST_F(SignedSettingsTest, RetrievePolicy) {
  std::string signed_serialized;
  em::PolicyFetchResponse signed_policy = BuildProto(fake_prop_,
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
  EXPECT_CALL(m_, StartVerifyAttempt(fake_prop_, fake_sig, _))
      .Times(1);

  s->Execute();
  message_loop_.RunAllPending();
  UnMockLoginLib();

  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
}

TEST_F(SignedSettingsTest, RetrieveNoPolicy) {
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

TEST_F(SignedSettingsTest, RetrieveUnsignedPolicy) {
  std::string serialized;
  em::PolicyFetchResponse policy = BuildProto(fake_prop_,
                                              std::string(),
                                              &serialized);
  ProtoDelegate d(policy);
  d.expect_failure(SignedSettings::OPERATION_FAILED);
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
  d.expect_failure(SignedSettings::OPERATION_FAILED);
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
}

}  // namespace chromeos
