// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/connection_manager.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/chromeos/android_sms/fake_connection_establisher.h"
#include "content/public/test/fake_service_worker_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace android_sms {

namespace {
const int64_t kDummyVersionId = 123l;
const int64_t kDummyVersionId2 = 456l;
}  // namespace

class ConnectionManagerTest : public testing::Test {
 protected:
  ConnectionManagerTest() = default;
  ~ConnectionManagerTest() override = default;

  void SetUp() override {
    std::unique_ptr<FakeConnectionEstablisher> fake_connection_establisher =
        std::make_unique<FakeConnectionEstablisher>();
    fake_connection_establisher_ = fake_connection_establisher.get();
    fake_service_worker_context_ =
        std::make_unique<content::FakeServiceWorkerContext>();
    connection_manager_ = std::make_unique<ConnectionManager>(
        fake_service_worker_context_.get(),
        std::move(fake_connection_establisher));
  }

  void VerifyNumEstablishConnectionCalls(size_t count) {
    ASSERT_EQ(
        count,
        fake_connection_establisher_->establish_connection_calls().size());
    for (content::ServiceWorkerContext* service_worker_context :
         fake_connection_establisher_->establish_connection_calls())
      EXPECT_EQ(fake_service_worker_context_.get(), service_worker_context);
  }

  content::FakeServiceWorkerContext* fake_service_worker_context() const {
    return fake_service_worker_context_.get();
  }

 private:
  FakeConnectionEstablisher* fake_connection_establisher_;
  std::unique_ptr<content::FakeServiceWorkerContext>
      fake_service_worker_context_;
  std::unique_ptr<ConnectionManager> connection_manager_;
  DISALLOW_COPY_AND_ASSIGN(ConnectionManagerTest);
};

TEST_F(ConnectionManagerTest, ConnectOnActivate) {
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that connection establishing is attempted on startup and
  // when a new version is activated.
  // startup + activate.
  VerifyNumEstablishConnectionCalls(2u);
}

TEST_F(ConnectionManagerTest, ConnectOnNoControllees) {
  // Notify Activation so that Connection manager is tracking the version ID.
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that connection establishing is attempted when there are no
  // controllees.
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + activate + no controllees.
  VerifyNumEstablishConnectionCalls(3u);
}

TEST_F(ConnectionManagerTest, IgnoreRedundantVersion) {
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Notify that current active version is now redundant.
  fake_service_worker_context()->NotifyObserversOnVersionRedundant(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that no connection establishing is attempted when there are no
  // controllees for the redundant version.
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + activate only.
  VerifyNumEstablishConnectionCalls(2u);
}

TEST_F(ConnectionManagerTest, ConnectOnNoControlleesWithNoActive) {
  // Verify that connection establishing is attempted when there are no
  // controllees for a version ID even if the activate notification was missed.
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + no controllees.
  VerifyNumEstablishConnectionCalls(2u);
}

TEST_F(ConnectionManagerTest, IgnoreOnNoControlleesInvalidId) {
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that OnNoControllees with different version id is ignored.
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId2, GetAndroidMessagesURL());
  // startup + activate only.
  VerifyNumEstablishConnectionCalls(2u);
}

TEST_F(ConnectionManagerTest, InvalidScope) {
  GURL invalid_scope("https://example.com");
  // Verify that OnVersionActivated and OnNoControllees with invalid scope
  // are ignored
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, invalid_scope);
  fake_service_worker_context()->NotifyObserversOnNoControllees(kDummyVersionId,
                                                                invalid_scope);
  VerifyNumEstablishConnectionCalls(1u);  // startup only, ignore others.

  // Verify that OnVersionRedundant with invalid scope is ignored
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());
  fake_service_worker_context()->NotifyObserversOnVersionRedundant(
      kDummyVersionId, invalid_scope);
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + activate + no controllees.
  VerifyNumEstablishConnectionCalls(3u);
}

}  // namespace android_sms

}  // namespace chromeos
