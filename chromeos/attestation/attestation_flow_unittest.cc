// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::DoDefault;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::Sequence;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

void DBusCallbackFalse(const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void DBusCallbackTrue(const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void DBusCallbackFail(const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_FAILURE, false));
}

void AsyncCallbackFalse(cryptohome::AsyncMethodCaller::Callback callback) {
  callback.Run(false, cryptohome::MOUNT_ERROR_NONE);
}

class FakeDBusData {
 public:
  explicit FakeDBusData(const std::string& data) : data_(data) {}

  void operator() (const CryptohomeClient::DataMethodCallback& callback) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true, data_));
  }

 private:
  std::string data_;
};

}  // namespace

class AttestationFlowTest : public testing::Test {
 protected:
  void Run() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  base::MessageLoop message_loop_;
};

TEST_F(AttestationFlowTest, GetCertificate) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  // Use DBusCallbackFalse so the full enrollment flow is triggered.
  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .InSequence(flow_order)
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  // Use StrictMock when we want to verify invocation frequency.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1)
      .InSequence(flow_order);

  scoped_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendEnrollRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest,
      _)).Times(1)
         .InSequence(flow_order);

  std::string fake_enroll_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest;
  fake_enroll_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationEnroll(_, fake_enroll_response, _))
      .Times(1)
      .InSequence(flow_order);

  EXPECT_CALL(
      async_caller,
      AsyncTpmAttestationCreateCertRequest(_,
                                           PROFILE_ENTERPRISE_USER_CERTIFICATE,
                                           "fake@test.com", "fake_origin", _))
          .Times(1)
          .InSequence(flow_order);

  EXPECT_CALL(*proxy, SendCertificateRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest,
      _)).Times(1)
         .InSequence(flow_order);

  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationFinishCertRequest(fake_cert_response,
                                                   KEY_USER,
                                                   "fake@test.com",
                                                   kEnterpriseUserKey,
                                                   _))
      .Times(1)
      .InSequence(flow_order);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(
      true,
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCert))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, "fake@test.com",
                      "fake_origin", true, mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_NoEK) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  // We're not expecting any server calls in this case; StrictMock will verify.
  scoped_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", true,
                      mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_EKRejected) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  scoped_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(false);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendEnrollRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest,
      _)).Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", true,
                      mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_FailEnroll) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1);
  std::string fake_enroll_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest;
  fake_enroll_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationEnroll(_, fake_enroll_response, _))
      .WillOnce(WithArgs<2>(Invoke(AsyncCallbackFalse)));

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  scoped_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendEnrollRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest,
      _)).Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", true,
                      mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetMachineCertificateAlreadyEnrolled) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationCreateCertRequest(
                  _, PROFILE_ENTERPRISE_MACHINE_CERTIFICATE, "", "", _))
      .Times(1);
  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationFinishCertRequest(fake_cert_response,
                                                   KEY_DEVICE,
                                                   "",
                                                   kEnterpriseMachineKey,
                                                   _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));

  scoped_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendCertificateRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest,
      _)).Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(
      true,
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCert)).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_MACHINE_CERTIFICATE, "", "", true,
                      mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_FailCreateCertRequest) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationCreateCertRequest(
                  _, PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));

  // We're not expecting any server calls in this case; StrictMock will verify.
  scoped_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", true,
                      mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_CertRequestRejected) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationCreateCertRequest(
                  _, PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));

  scoped_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(false);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendCertificateRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest,
      _)).Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", true,
                      mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_FailIsEnrolled) {
  // We're not expecting any async calls in this case; StrictMock will verify.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFail));

  // We're not expecting any server calls in this case; StrictMock will verify.
  scoped_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", true,
                      mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_CheckExisting) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationCreateCertRequest(
                  _, PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", _))
      .Times(1);
  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationFinishCertRequest(fake_cert_response,
                                                   KEY_USER,
                                                   "",
                                                   kEnterpriseUserKey,
                                                   _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));
  EXPECT_CALL(client,
              TpmAttestationDoesKeyExist(KEY_USER, "", kEnterpriseUserKey, _))
      .WillRepeatedly(WithArgs<3>(Invoke(DBusCallbackFalse)));

  scoped_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(*proxy, SendCertificateRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest,
      _)).Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(
      true,
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCert)).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", false,
                      mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_AlreadyExists) {
  // We're not expecting any async calls in this case; StrictMock will verify.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));
  EXPECT_CALL(client,
              TpmAttestationDoesKeyExist(KEY_USER, "", kEnterpriseUserKey, _))
      .WillRepeatedly(WithArgs<3>(Invoke(DBusCallbackTrue)));
  EXPECT_CALL(client,
              TpmAttestationGetCertificate(KEY_USER, "", kEnterpriseUserKey, _))
      .WillRepeatedly(WithArgs<3>(Invoke(FakeDBusData("fake_cert"))));

  // We're not expecting any server calls in this case; StrictMock will verify.
  scoped_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(true, "fake_cert")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", false,
                      mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, AlternatePCA) {
  // Strategy: Create a ServerProxy mock which reports ALTERNATE_PCA and check
  // that all calls to the AsyncMethodCaller reflect this PCA type.
  scoped_ptr<MockServerProxy> proxy(new NiceMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(Return(ALTERNATE_PCA));

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  NiceMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationCreateEnrollRequest(ALTERNATE_PCA, _))
      .Times(AtLeast(1));
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationEnroll(ALTERNATE_PCA, _, _))
      .Times(AtLeast(1));
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationCreateCertRequest(ALTERNATE_PCA, _, _, _, _))
      .Times(AtLeast(1));

  NiceMock<MockObserver> observer;
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  scoped_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, proxy_interface.Pass());
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, "", "", true,
                      mock_callback);
  Run();
}

}  // namespace attestation
}  // namespace chromeos
