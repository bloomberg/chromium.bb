// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ownership_service.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/chromeos/login/mock_owner_key_utils.h"
#include "chrome/browser/chromeos/login/owner_manager_unittest.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "crypto/nss_util.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using ::crypto::RSAPrivateKey;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::_;


namespace chromeos {

class OwnershipServiceTest : public testing::Test {
 public:
  OwnershipServiceTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE),
        mock_(new MockKeyUtils),
        injector_(mock_) /* injector_ takes ownership of mock_ */ {
  }
  virtual ~OwnershipServiceTest() {}

  virtual void SetUp() {
    crypto::OpenPersistentNSSDB();  // TODO(cmasone): use test DB instead
    fake_private_key_.reset(RSAPrivateKey::Create(256));
    ASSERT_TRUE(fake_private_key_->ExportPublicKey(&fake_public_key_));

    // Mimic ownership.
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir_.path(), &tmpfile_));

    file_thread_.Start();
    OwnerKeyUtils::set_factory(&injector_);
    service_.reset(new OwnershipService);  // must happen AFTER set_factory().
    service_->Prewarm();
  }

  virtual void TearDown() {
    OwnerKeyUtils::set_factory(NULL);
    service_.reset(NULL);
  }

  void StartUnowned() {
    file_util::Delete(tmpfile_, false);
  }

  ScopedTempDir tmpdir_;
  FilePath tmpfile_;

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  std::vector<uint8> fake_public_key_;
  scoped_ptr<RSAPrivateKey> fake_private_key_;

  MockKeyUtils* mock_;
  MockInjector injector_;
  scoped_ptr<OwnershipService> service_;
};

TEST_F(OwnershipServiceTest, IsOwned) {
  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_TRUE(service_->IsAlreadyOwned());
}

TEST_F(OwnershipServiceTest, IsOwnershipTaken) {
  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_TRUE(service_->GetStatus(true) == OwnershipService::OWNERSHIP_TAKEN);
}

TEST_F(OwnershipServiceTest, IsUnowned) {
  StartUnowned();

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_FALSE(service_->IsAlreadyOwned());
}

TEST_F(OwnershipServiceTest, IsOwnershipNone) {
  StartUnowned();

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_TRUE(service_->GetStatus(true) == OwnershipService::OWNERSHIP_NONE);
}

TEST_F(OwnershipServiceTest, LoadOwnerKeyFail) {
  base::WaitableEvent event(true, false);
  MockKeyLoadObserver loader(&event);
  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  service_->StartLoadOwnerKeyAttempt();

  // Run remaining events, until ExportPublicKeyViaDbus().
  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnershipServiceTest, UpdateOwnerKey) {
  base::WaitableEvent event(true, false);
  MockKeyUpdateUser delegate(&event);
  service_->StartUpdateOwnerKey(std::vector<uint8>(), &delegate);

  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnershipServiceTest, LoadOwnerKey) {
  base::WaitableEvent event(true, false);
  MockKeyLoadObserver loader(&event);
  loader.ExpectKeyFetchSuccess(true);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(fake_public_key_),
                      Return(true)))
      .RetiresOnSaturation();
  service_->StartLoadOwnerKeyAttempt();

  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnershipServiceTest, NotYetOwnedVerify) {
  StartUnowned();

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));

  base::WaitableEvent event(true, false);
  MockKeyUser delegate(OwnerManager::KEY_UNAVAILABLE, &event);
  service_->StartVerifyAttempt("", std::vector<uint8>(), &delegate);

  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnershipServiceTest, GetKeyFailDuringVerify) {
  MockKeyLoadObserver loader(NULL);
  loader.ExpectKeyFetchSuccess(false);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  base::WaitableEvent event(true, false);
  MockKeyUser delegate(OwnerManager::KEY_UNAVAILABLE, &event);
  service_->StartVerifyAttempt("", std::vector<uint8>(), &delegate);

  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnershipServiceTest, GetKeyAndVerify) {
  MockKeyLoadObserver loader(NULL);
  loader.ExpectKeyFetchSuccess(true);

  std::string data;
  std::vector<uint8> sig(0, 2);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(fake_public_key_),
                      Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_, Verify(Eq(data), Eq(sig), Eq(fake_public_key_)))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  base::WaitableEvent event(true, false);
  MockKeyUser delegate(OwnerManager::SUCCESS, &event);
  service_->StartVerifyAttempt(data, sig, &delegate);

  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnershipServiceTest, GetKeyAndFailVerify) {
  MockKeyLoadObserver loader(NULL);
  loader.ExpectKeyFetchSuccess(true);

  std::string data;
  std::vector<uint8> sig(0, 2);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(fake_public_key_),
                      Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_, Verify(Eq(data), Eq(sig), Eq(fake_public_key_)))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  base::WaitableEvent event(true, false);
  MockKeyUser delegate(OwnerManager::OPERATION_FAILED, &event);
  service_->StartVerifyAttempt(data, sig, &delegate);

  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

}  // namespace chromeos
