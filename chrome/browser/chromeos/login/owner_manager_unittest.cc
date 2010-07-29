// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_manager.h"

#include <cert.h>
#include <keyhi.h>
#include <keythi.h>  // KeyType enum
#include <pk11pub.h>
#include <stdlib.h>

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/nss_util_internal.h"
#include "base/nss_util.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::_;

namespace chromeos {

namespace {

class MockKeyUtils : public OwnerKeyUtils {
 public:
  MockKeyUtils() {}
  ~MockKeyUtils() {}
  MOCK_METHOD2(GenerateKeyPair, bool(SECKEYPrivateKey** private_key_out,
                                     SECKEYPublicKey** public_key_out));
  MOCK_METHOD1(ExportPublicKeyViaDbus, bool(SECKEYPublicKey* key));
  MOCK_METHOD2(ExportPublicKeyToFile, bool(SECKEYPublicKey* key,
                                           const FilePath& key_file));
  MOCK_METHOD1(ImportPublicKey, SECKEYPublicKey*(const FilePath& key_file));
  MOCK_METHOD1(FindPrivateKey, SECKEYPrivateKey*(SECKEYPublicKey* key));
  MOCK_METHOD2(DestroyKeys, void(SECKEYPrivateKey* private_key,
                                 SECKEYPublicKey* public_key));
  MOCK_METHOD0(GetOwnerKeyFilePath, FilePath());
};

class MockInjector : public OwnerKeyUtils::Factory {
 public:
  // Takes ownership of |mock|.
  explicit MockInjector(MockKeyUtils* mock) :
      transient_(mock),
      delete_transient_(true) {
  }

  virtual ~MockInjector() {
    if (delete_transient_)
      delete transient_;
  }

  // If this is called, its caller takes ownership of |transient_|.
  // If it's never called, |transient_| remains our problem.
  OwnerKeyUtils* CreateOwnerKeyUtils() {
    delete_transient_ = false;
    return transient_;
  }

 private:
  MockKeyUtils* transient_;
  bool delete_transient_;
};

class KeyUser : public OwnerManager::Delegate {
 public:
  explicit KeyUser(const OwnerManager::KeyOpCode expected)
      : expected_(expected) {
  }

  virtual ~KeyUser() {}

  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::string& payload) {
    MessageLoop::current()->Quit();
    EXPECT_EQ(expected_, return_code);
  }

  const OwnerManager::KeyOpCode expected_;
};

static bool Win(SECKEYPublicKey* key) {
  MessageLoop::current()->Quit();
  return true;
}

static bool Fail(SECKEYPublicKey* key) {
  MessageLoop::current()->Quit();
  return false;
}

}  // anonymous namespace

class OwnerManagerTest : public ::testing::Test,
                         public NotificationObserver {
 public:
  OwnerManagerTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(ChromeThread::UI, &message_loop_),
        file_thread_(ChromeThread::FILE),
        fake_public_key_(reinterpret_cast<SECKEYPublicKey*>(7)),
        fake_private_key_(reinterpret_cast<SECKEYPrivateKey*>(7)),
        success_expected_(false),
        quit_on_observe_(true),
        mock_(new MockKeyUtils),
        injector_(mock_) /* injector_ takes ownership of mock_ */ {
    registrar_.Add(
        this,
        NotificationType::OWNER_KEY_FETCH_ATTEMPT_COMPLETE,
        NotificationService::AllSources());
  }
  virtual ~OwnerManagerTest() {}

  virtual void SetUp() {
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
    manager->private_key_ = fake_private_key_;
  }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::OWNER_KEY_FETCH_ATTEMPT_COMPLETE) {
      EXPECT_EQ(success_expected_,
                NULL != *Details<SECKEYPublicKey*>(details).ptr());
      if (quit_on_observe_)
        MessageLoop::current()->Quit();
    }
  }

  void ExpectKeyFetchSuccess(bool should_succeed) {
    success_expected_ = should_succeed;
  }
  void SetQuitOnKeyFetch(bool should_quit) { quit_on_observe_ = should_quit; }

  ScopedTempDir tmpdir_;
  FilePath tmpfile_;

  MessageLoop message_loop_;
  ChromeThread ui_thread_;
  ChromeThread file_thread_;

  SECKEYPublicKey* fake_public_key_;
  SECKEYPrivateKey* fake_private_key_;

  NotificationRegistrar registrar_;
  bool success_expected_;
  bool quit_on_observe_;

  MockKeyUtils* mock_;
  MockInjector injector_;
};

TEST_F(OwnerManagerTest, LoadKeyUnowned) {
  StartUnowned();

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  EXPECT_FALSE(manager->StartLoadOwnerKeyAttempt());
}

TEST_F(OwnerManagerTest, LoadOwnerKeyFail) {
  SECKEYPublicKey* to_return = NULL;

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_))
      .WillOnce(Return(to_return))
      .RetiresOnSaturation();

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  EXPECT_TRUE(manager->StartLoadOwnerKeyAttempt());

  // Run remaining events, until ExportPublicKeyViaDbus().
  message_loop_.Run();
}

TEST_F(OwnerManagerTest, LoadOwnerKey) {
  ExpectKeyFetchSuccess(true);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_))
      .WillOnce(Return(fake_public_key_))
      .RetiresOnSaturation();

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  EXPECT_TRUE(manager->StartLoadOwnerKeyAttempt());

  // Run remaining events, until ExportPublicKeyViaDbus().
  message_loop_.Run();
}

TEST_F(OwnerManagerTest, TakeOwnershipAlreadyOwned) {

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  EXPECT_FALSE(manager->StartTakeOwnershipAttempt());
}

TEST_F(OwnerManagerTest, KeyGenerationFail) {
  StartUnowned();

  EXPECT_CALL(*mock_, GenerateKeyPair(_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  EXPECT_TRUE(manager->StartTakeOwnershipAttempt());

  message_loop_.Run();
}

TEST_F(OwnerManagerTest, KeyExportFail) {
  StartUnowned();

  EXPECT_CALL(*mock_, GenerateKeyPair(_, _))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_, ExportPublicKeyViaDbus(_))
      .WillOnce(Invoke(Fail))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_, DestroyKeys(_, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  EXPECT_TRUE(manager->StartTakeOwnershipAttempt());

  message_loop_.Run();
}

TEST_F(OwnerManagerTest, TakeOwnership) {
  StartUnowned();
  ExpectKeyFetchSuccess(true);

  EXPECT_CALL(*mock_, GenerateKeyPair(_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(fake_public_key_),
                      Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_, ExportPublicKeyViaDbus(_))
      .WillOnce(Invoke(Win))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  EXPECT_TRUE(manager->StartTakeOwnershipAttempt());

  message_loop_.Run();
}

TEST_F(OwnerManagerTest, NotYetOwnedVerify) {
  StartUnowned();

  // Since this shouldn't happen, don't want it to end the test if it does.
  SetQuitOnKeyFetch(false);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  KeyUser delegate(OwnerManager::KEY_UNAVAILABLE);
  EXPECT_FALSE(manager->StartVerifyAttempt("", "", &delegate));
}

TEST_F(OwnerManagerTest, AlreadyHaveKeysVerify) {
  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  InjectKeys(manager.get());
  KeyUser delegate(OwnerManager::SUCCESS);
  EXPECT_TRUE(manager->StartVerifyAttempt("", "", &delegate));

  message_loop_.Run();
}

TEST_F(OwnerManagerTest, GetKeyFailDuringVerify) {
  ExpectKeyFetchSuccess(false);
  SECKEYPublicKey* to_return = NULL;

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_))
      .WillOnce(Return(to_return))
      .RetiresOnSaturation();

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  KeyUser delegate(OwnerManager::KEY_UNAVAILABLE);
  EXPECT_TRUE(manager->StartVerifyAttempt("", "", &delegate));

  message_loop_.Run();
}

TEST_F(OwnerManagerTest, GetKeyAndVerify) {
  ExpectKeyFetchSuccess(true);
  SetQuitOnKeyFetch(false);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_))
      .WillOnce(Return(fake_public_key_))
      .RetiresOnSaturation();

  scoped_refptr<OwnerManager> manager(new OwnerManager);
  KeyUser delegate(OwnerManager::SUCCESS);
  EXPECT_TRUE(manager->StartVerifyAttempt("", "", &delegate));

  message_loop_.Run();
}

}  // namespace chromeos
