// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Sequence;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

void DBusCallbackFalse(const BoolDBusMethodCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void DBusCallbackTrue(const BoolDBusMethodCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void DBusCallbackFail(const BoolDBusMethodCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_FAILURE, false));
}

void AsyncCallbackFalse(cryptohome::AsyncMethodCaller::Callback callback) {
  callback.Run(false, cryptohome::MOUNT_ERROR_NONE);
}

}  // namespace

class AttestationFlowTest : public testing::Test {
 protected:
  void Run() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  MessageLoop message_loop_;
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
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_))
      .Times(1)
      .InSequence(flow_order);

  StrictMock<MockServerProxy> proxy;
  proxy.DeferToFake(true);
  EXPECT_CALL(proxy, SendEnrollRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest,
      _)).Times(1)
         .InSequence(flow_order);

  std::string fake_enroll_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest;
  fake_enroll_response += "_response";
  EXPECT_CALL(async_caller, AsyncTpmAttestationEnroll(fake_enroll_response, _))
      .Times(1)
      .InSequence(flow_order);

  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(false, _))
      .Times(1)
      .InSequence(flow_order);

  EXPECT_CALL(proxy, SendCertificateRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest,
      _)).Times(1)
         .InSequence(flow_order);

  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationFinishCertRequest(fake_cert_response, _))
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

  AttestationFlow flow(&async_caller, &client, &proxy);
  flow.GetCertificate("test", mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_NoEK) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  // We're not expecting any server calls in this case; StrictMock will verify.
  StrictMock<MockServerProxy> proxy;

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  AttestationFlow flow(&async_caller, &client, &proxy);
  flow.GetCertificate("test", mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_EKRejected) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  StrictMock<MockServerProxy> proxy;
  proxy.DeferToFake(false);
  EXPECT_CALL(proxy, SendEnrollRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest,
      _)).Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, ""))
      .Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  AttestationFlow flow(&async_caller, &client, &proxy);
  flow.GetCertificate("test", mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_FailEnroll) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateEnrollRequest(_))
      .Times(1);
  std::string fake_enroll_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest;
  fake_enroll_response += "_response";
  EXPECT_CALL(async_caller, AsyncTpmAttestationEnroll(fake_enroll_response, _))
      .WillOnce(WithArgs<1>(Invoke(AsyncCallbackFalse)));

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFalse));

  StrictMock<MockServerProxy> proxy;
  proxy.DeferToFake(true);
  EXPECT_CALL(proxy, SendEnrollRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationEnrollRequest,
      _)).Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  AttestationFlow flow(&async_caller, &client, &proxy);
  flow.GetCertificate("test", mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetOwnerCertificateAlreadyEnrolled) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(true, _))
      .Times(1);
  std::string fake_cert_response =
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest;
  fake_cert_response += "_response";
  EXPECT_CALL(async_caller,
              AsyncTpmAttestationFinishCertRequest(fake_cert_response, _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));

  StrictMock<MockServerProxy> proxy;
  proxy.DeferToFake(true);
  EXPECT_CALL(proxy, SendCertificateRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest,
      _)).Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(
      true,
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCert)).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  AttestationFlow flow(&async_caller, &client, &proxy);
  flow.GetCertificate("attest-ent-machine", mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_FailCreateCertRequest) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(false, _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));

  // We're not expecting any server calls in this case; StrictMock will verify.
  StrictMock<MockServerProxy> proxy;

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  AttestationFlow flow(&async_caller, &client, &proxy);
  flow.GetCertificate("test", mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_CertRequestRejected) {
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;
  async_caller.SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(async_caller, AsyncTpmAttestationCreateCertRequest(false, _))
      .Times(1);

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackTrue));

  StrictMock<MockServerProxy> proxy;
  proxy.DeferToFake(false);
  EXPECT_CALL(proxy, SendCertificateRequest(
      cryptohome::MockAsyncMethodCaller::kFakeAttestationCertRequest,
      _)).Times(1);

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  AttestationFlow flow(&async_caller, &client, &proxy);
  flow.GetCertificate("test", mock_callback);
  Run();
}

TEST_F(AttestationFlowTest, GetCertificate_FailIsEnrolled) {
  // We're not expecting any server calls in this case; StrictMock will verify.
  StrictMock<cryptohome::MockAsyncMethodCaller> async_caller;

  chromeos::MockCryptohomeClient client;
  EXPECT_CALL(client, TpmAttestationIsEnrolled(_))
      .WillRepeatedly(Invoke(DBusCallbackFail));

  // We're not expecting any server calls in this case; StrictMock will verify.
  StrictMock<MockServerProxy> proxy;

  StrictMock<MockObserver> observer;
  EXPECT_CALL(observer, MockCertificateCallback(false, "")).Times(1);
  AttestationFlow::CertificateCallback mock_callback = base::Bind(
      &MockObserver::MockCertificateCallback,
      base::Unretained(&observer));

  AttestationFlow flow(&async_caller, &client, &proxy);
  flow.GetCertificate("test", mock_callback);
  Run();
}

}  // namespace attestation
}  // namespace chromeos
