// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_manager.h"
#include "chrome/browser/chromeos/login/owner_manager_unittest.h"

#include <string>

#include "base/crypto/rsa_private_key.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/nss_util.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/login/mock_owner_key_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::RSAPrivateKey;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::_;

namespace chromeos {

class OwnerManagerTest : public ::testing::Test {
 public:
  OwnerManagerTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE),
        mock_(new MockKeyUtils),
        injector_(mock_) /* injector_ takes ownership of mock_ */ {
  }
  virtual ~OwnerManagerTest() {}

  virtual void SetUp() {
    base::OpenPersistentNSSDB();  // TODO(cmasone): use test DB instead
    fake_private_key_.reset(RSAPrivateKey::Create(256));
    ASSERT_TRUE(fake_private_key_->ExportPublicKey(&fake_public_key_));

    // Mimic ownership.
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir_.path(), &tmpfile_));

    file_thread_.Start();
    OwnerKeyUtils::set_factory(&injector_);
  }

  virtual void TearDown() {
    OwnerKeyUtils::set_factory(NULL);
  }

  void StartUnowned() {
    file_util::Delete(tmpfile_, false);
  }

  void InjectKeys(OwnerManager* manager) {
    manager->public_key_ = fake_public_key_;
    manager->private_key_.reset(fake_private_key_.release());
  }

  ScopedTempDir tmpdir_;
  FilePath tmpfile_;

  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  std::vector<uint8> fake_public_key_;
  scoped_ptr<RSAPrivateKey> fake_private_key_;

  MockKeyUtils* mock_;
  MockInjector injector_;

};

TEST_F(OwnerManagerTest, LoadOwnerKeyFail) {
  StartUnowned();
  MockKeyLoadObserver loader;
  scoped_refptr<OwnerManager> manager(new OwnerManager);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(manager.get(),
                        &OwnerManager::LoadOwnerKey));
  message_loop_.Run();
}

TEST_F(OwnerManagerTest, AlreadyLoadedOwnerKey) {
  MockKeyLoadObserver loader;
  loader.ExpectKeyFetchSuccess(true);
  scoped_refptr<OwnerManager> manager(new OwnerManager);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));

  InjectKeys(manager.get());

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(manager.get(),
                        &OwnerManager::LoadOwnerKey));

  message_loop_.Run();
}

TEST_F(OwnerManagerTest, LoadOwnerKey) {
  MockKeyLoadObserver loader;
  loader.ExpectKeyFetchSuccess(true);
  scoped_refptr<OwnerManager> manager(new OwnerManager);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(fake_public_key_),
                      Return(true)))
      .RetiresOnSaturation();

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(manager.get(),
                        &OwnerManager::LoadOwnerKey));

  message_loop_.Run();
}

TEST_F(OwnerManagerTest, GetKeyFailDuringVerify) {
  StartUnowned();
  MockKeyLoadObserver loader;
  loader.ExpectKeyFetchSuccess(false);
  scoped_refptr<OwnerManager> manager(new OwnerManager);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  MockKeyUser delegate(OwnerManager::KEY_UNAVAILABLE);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(manager.get(),
                        &OwnerManager::Verify,
                        BrowserThread::UI,
                        std::string(),
                        std::vector<uint8>(),
                        &delegate));
  message_loop_.Run();
}

TEST_F(OwnerManagerTest, AlreadyHaveKeysVerify) {
  scoped_refptr<OwnerManager> manager(new OwnerManager);

  std::string data;
  std::vector<uint8> sig(0, 2);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, Verify(Eq(data), Eq(sig), Eq(fake_public_key_)))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  InjectKeys(manager.get());
  MockKeyUser delegate(OwnerManager::SUCCESS);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(manager.get(),
                        &OwnerManager::Verify,
                        BrowserThread::UI,
                        data,
                        sig,
                        &delegate));
  message_loop_.Run();
}

TEST_F(OwnerManagerTest, GetKeyAndVerify) {
  MockKeyLoadObserver loader;
  loader.ExpectKeyFetchSuccess(true);
  loader.SetQuitOnKeyFetch(false);
  scoped_refptr<OwnerManager> manager(new OwnerManager);

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

  MockKeyUser delegate(OwnerManager::SUCCESS);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(manager.get(),
                        &OwnerManager::Verify,
                        BrowserThread::UI,
                        data,
                        sig,
                        &delegate));
  message_loop_.Run();
}

TEST_F(OwnerManagerTest, AlreadyHaveKeysSign) {
  scoped_refptr<OwnerManager> manager(new OwnerManager);

  std::string data;
  std::vector<uint8> sig(0, 2);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, Sign(Eq(data), _, Eq(fake_private_key_.get())))
      .WillOnce(DoAll(SetArgumentPointee<1>(sig),
                      Return(true)))
      .RetiresOnSaturation();

  InjectKeys(manager.get());
  MockSigner delegate(OwnerManager::SUCCESS, sig);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(manager.get(),
                        &OwnerManager::Sign,
                        BrowserThread::UI,
                        data,
                        &delegate));
  message_loop_.Run();
}

}  // namespace chromeos
