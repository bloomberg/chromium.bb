// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "components/signin/core/account_id/account_id.h"
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void DBusCallbackTrue(const BoolDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void DBusCallbackFail(const BoolDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_FAILURE, false));
}

void AsyncCallbackFalse(cryptohome::AsyncMethodCaller::Callback callback) {
  callback.Run(false, cryptohome::MOUNT_ERROR_NONE);
}

class FakeDBusData {
 public:
  explicit FakeDBusData(const std::string& data) : data_(data) {}

  void operator() (const CryptohomeClient::DataMethodCallback& callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true, data_));
  }

 private:
  std::string data_;
};

}  // namespace

class AttestationFlowTest : public testing::Test {
 public:
  void QuitRunLoopCertificateCallback(
      const AttestationFlow::CertificateCallback& callback,
      bool success,
      const std::string& cert) {
    LOG(WARNING) << "Quitting run loop.";
    run_loop_->Quit();
    if (!callback.is_null())
      callback.Run(success, cert);
  }

 protected:
  void RunUntilIdle() {
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop_->RunUntilIdle();
  }

  void Run() {
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop_->Run();
  }

  base::MessageLoop message_loop_;
  base::RunLoop* run_loop_;
};

TEST_F(AttestationFlowTest, GetCertificate) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  // Use DBusCallbackFalse so the full enrollment flow is triggered.
  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .InSequence(flow_order)
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  EXPECT_CALL(client, TpmAttestationIsPrepared(_))
      .InSequence(flow_order)
      .WillOnce(Invoke(DBusCallbackTrue));

  // Use StrictMock when we want to verify invocation frequency.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1)
      .InSequence(flow_order);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());
  EXPECT_CALL(
      *proxy,
      SendEnrollRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest, _))
      .Times(1)
      .InSequence(flow_order);

  std::string fake_enroll_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest;
  fake_enroll_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationEnroll(_, fake_enroll_response, _))
      .Times(1)
      .InSequence(flow_order);

  const AccountId account_id = AccountId::FromUserEmail("fake@test.com");
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationCreateCertRequest(
                  _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                  cryptohome::Identification(account_id), "fake_origin", _))
      .Times(1)
      .InSequence(flow_order);

  EXPECT_CALL(
      *proxy,
      SendCertificateRequest(
          cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest, _))
      .Times(1)
      .InSequence(flow_order);

  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller, AsyncTpmAttestationFinishCertRequest(
                                fake_cert_response, KEY_USER,
                                cryptohome::Identification(account_id),
                                kEnterpriseUserKey, _))
      .Times(1)
      .InSequence(flow_order);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(
      observer,
      MockCertificateCallback(
          true, cryptohome::MockAsyncMethodCaller::kFakeAttestationCert))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback, base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, account_id,
                      "fake_origin", true, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_Attestation_Not_Prepared) {
  // Verify the order of calls in a sequence.
  Sequence flow_order;

  // Use DBusCallbackFalse so the full enrollment flow is triggered.
  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .InSequence(flow_order)
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  // It will take a bit for attestation to be prepared.
  EXPECT_CALL(client, TpmAttestationIsPrepared(_))
      .InSequence(flow_order)
      .WillOnce(Invoke(DBusCallbackFalse))
      .WillOnce(Invoke(DBusCallbackTrue));

  // Use StrictMock when we want to verify invocation frequency.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1)
      .InSequence(flow_order);

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
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

  const AccountId account_id = AccountId::FromUserEmail("fake@test.com");
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationCreateCertRequest(
                  _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                  cryptohome::Identification(account_id), "fake_origin", _))
      .Times(1)
      .InSequence(flow_order);

  EXPECT_CALL(*proxy, SendCertificateRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest,
      _)).Times(1)
         .InSequence(flow_order);

  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller, AsyncTpmAttestationFinishCertRequest(
                                fake_cert_response, KEY_USER,
                                cryptohome::Identification(account_id),
                                kEnterpriseUserKey, _));

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(
      true,
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCert))
      .Times(1)
      .InSequence(flow_order);
  AttestationFlow::CertificateCallback callback = base::Bind(
      &AttestationFlowTest::QuitRunLoopCertificateCallback,
      base::Unretained(this), base::Bind(&MockObserver::MockCertificateCallback,
                                         base::Unretained(&observer)));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.set_retry_delay(base::TimeDelta::FromMilliseconds(30));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, account_id,
                      "fake_origin", true, callback);

  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_Attestation_Never_Prepared) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(false, cryptohome::MOUNT_ERROR_NONE);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  EXPECT_CALL(client, TpmAttestationIsPrepared(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback callback = base::Bind(
      &AttestationFlowTest::QuitRunLoopCertificateCallback,
      base::Unretained(this), base::Bind(&MockObserver::MockCertificateCallback,
                                         base::Unretained(&observer)));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.set_ready_timeout(base::TimeDelta::FromMilliseconds(20));
  flow.set_retry_delay(base::TimeDelta::FromMilliseconds(6));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(),
                      "fake_origin", true, callback);

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

  EXPECT_CALL(client, TpmAttestationIsPrepared(_))
      .WillOnce(Invoke(DBusCallbackTrue));

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_EKRejected) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  EXPECT_CALL(client, TpmAttestationIsPrepared(_))
      .WillOnce(Invoke(DBusCallbackTrue));

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
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

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, mock_callback);
  RunUntilIdle();
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

  EXPECT_CALL(client, TpmAttestationIsPrepared(_))
      .WillOnce(Invoke(DBusCallbackTrue));

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
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

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetMachineCertificateAlreadyEnrolled) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(
                                _, PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
                                cryptohome::Identification(), "", _))
      .Times(1);
  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationFinishCertRequest(
                  fake_cert_response, KEY_DEVICE, cryptohome::Identification(),
                  kEnterpriseMachineKey, _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
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

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_MACHINE_CERTIFICATE, EmptyAccountId(),
                      "", true, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_FailCreateCertRequest) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(
                                _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                                cryptohome::Identification(), "", _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_CertRequestRejected) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(
                                _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                                cryptohome::Identification(), "", _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
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

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_FailIsEnrolled) {
  // We're not expecting any async calls in this case; StrictMock will verify.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFail));

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_CheckExisting) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(
                                _, PROFILE_ENTERPRISE_USER_CERTIFICATE,
                                cryptohome::Identification(), "", _))
      .Times(1);
  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationFinishCertRequest(fake_cert_response, KEY_USER,
                                                   cryptohome::Identification(),
                                                   kEnterpriseUserKey, _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));
  EXPECT_CALL(client,
              TpmAttestationDoesKeyExist(KEY_USER, cryptohome::Identification(),
                                         kEnterpriseUserKey, _))
      .WillRepeatedly(WithArgs<3>(Invoke(DBusCallbackFalse)));

  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
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

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      false, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, GetCertificate_AlreadyExists) {
  // We're not expecting any async calls in this case; StrictMock will verify.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));
  EXPECT_CALL(client,
              TpmAttestationDoesKeyExist(KEY_USER, cryptohome::Identification(),
                                         kEnterpriseUserKey, _))
      .WillRepeatedly(WithArgs<3>(Invoke(DBusCallbackTrue)));
  EXPECT_CALL(client, TpmAttestationGetCertificate(KEY_USER,
                                                   cryptohome::Identification(),
                                                   kEnterpriseUserKey, _))
      .WillRepeatedly(WithArgs<3>(Invoke(FakeDBusData("fake_cert"))));

  // We're not expecting any server calls in this case; StrictMock will verify.
  std::unique_ptr<MockServerProxy> proxy(new StrictMock<MockServerProxy>());
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(DoDefault());

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(true, "fake_cert")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      false, mock_callback);
  RunUntilIdle();
}

TEST_F(AttestationFlowTest, AlternatePCA) {
  // Strategy: Create a ServerProxy mock which reports ALTERNATE_PCA and check
  // that all calls to the AsyncMethodCaller reflect this PCA type.
  std::unique_ptr<MockServerProxy> proxy(new NiceMock<MockServerProxy>());
  proxy->DeferToFake(true);
  EXPECT_CALL(*proxy, GetType()).WillRepeatedly(Return(ALTERNATE_PCA));

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  EXPECT_CALL(client, TpmAttestationIsPrepared(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));

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

  std::unique_ptr<ServerProxy> proxy_interface(proxy.release());
  AttestationFlow flow(&async_caller, &client, std::move(proxy_interface));
  flow.GetCertificate(PROFILE_ENTERPRISE_USER_CERTIFICATE, EmptyAccountId(), "",
                      true, mock_callback);
  RunUntilIdle();
}

}  // namespace attestation
}  // namespace chromeos
