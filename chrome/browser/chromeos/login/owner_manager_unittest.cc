// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_manager.h"
#include "chrome/browser/chromeos/login/owner_manager_unittest.h"

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/chromeos/login/mock_owner_key_utils.h"
#include "chrome/common/chrome_notification_types.h"
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

////////////////////////////////////////////////////////////////////////////////
// MockKeyLoadObserver
MockKeyLoadObserver::MockKeyLoadObserver(base::WaitableEvent* e)
    : success_expected_(false),
      event_(e),
      observed_(false) {
  registrar_.Add(
      this,
      chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_FAILED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,
      content::NotificationService::AllSources());
}

MockKeyLoadObserver::~MockKeyLoadObserver() {
  DCHECK(observed_);
}

void MockKeyLoadObserver::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  LOG(INFO) << "Observed key fetch event";
  if (type == chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED) {
    DCHECK(success_expected_);
    observed_ = true;
    if (event_)
      event_->Signal();
  } else if (type == chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_FAILED) {
    DCHECK(!success_expected_);
    observed_ = true;
    if (event_)
      event_->Signal();
  }
}

void MockKeyLoadObserver::ExpectKeyFetchSuccess(bool should_succeed) {
  success_expected_ = should_succeed;
}

////////////////////////////////////////////////////////////////////////////////
// MockKeyUser
MockKeyUser::MockKeyUser(const OwnerManager::KeyOpCode expected,
                         base::WaitableEvent* e)
    : expected_(expected),
      event_(e) {
}

void MockKeyUser::OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                                  const std::vector<uint8>& payload) {
  DCHECK_EQ(expected_, return_code);
  if (event_)
    event_->Signal();
}

////////////////////////////////////////////////////////////////////////////////
// MockKeyUpdateUser

void MockKeyUpdateUser::OnKeyUpdated() {
  if (event_)
    event_->Signal();
}

////////////////////////////////////////////////////////////////////////////////
// MockSigner

MockSigner::MockSigner(const OwnerManager::KeyOpCode expected,
                       const std::vector<uint8>& sig,
                       base::WaitableEvent* e)
    : expected_code_(expected),
      expected_sig_(sig),
      event_(e) {
}

MockSigner::~MockSigner() {}

void MockSigner::OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                                 const std::vector<uint8>& payload) {
  DCHECK_EQ(expected_code_, return_code);
  for (uint32 i = 0; i < payload.size(); ++i)
    DCHECK_EQ(expected_sig_[i], payload[i]);
  if (event_)
    event_->Signal();
}

////////////////////////////////////////////////////////////////////////////////
// OwnerManagerTest

class OwnerManagerTest : public testing::Test {
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
    crypto::OpenPersistentNSSDB();  // TODO(cmasone): use test DB instead
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
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  std::vector<uint8> fake_public_key_;
  scoped_ptr<RSAPrivateKey> fake_private_key_;

  MockKeyUtils* mock_;
  MockInjector injector_;
};

TEST_F(OwnerManagerTest, UpdateOwnerKey) {
  scoped_refptr<OwnerManager> manager(new OwnerManager);

  base::WaitableEvent event(true, false);
  MockKeyUpdateUser delegate(&event);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OwnerManager::UpdateOwnerKey, manager.get(),
                 BrowserThread::UI, std::vector<uint8>(), &delegate));
  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnerManagerTest, LoadOwnerKeyFail) {
  StartUnowned();
  base::WaitableEvent event(true, false);
  MockKeyLoadObserver loader(&event);
  scoped_refptr<OwnerManager> manager(new OwnerManager);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OwnerManager::LoadOwnerKey, manager.get()));
  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnerManagerTest, AlreadyLoadedOwnerKey) {
  base::WaitableEvent event(true, false);
  MockKeyLoadObserver loader(&event);
  loader.ExpectKeyFetchSuccess(true);
  scoped_refptr<OwnerManager> manager(new OwnerManager);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));

  InjectKeys(manager.get());

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OwnerManager::LoadOwnerKey, manager.get()));
  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnerManagerTest, LoadOwnerKey) {
  base::WaitableEvent event(true, false);
  MockKeyLoadObserver loader(&event);
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
      base::Bind(&OwnerManager::LoadOwnerKey, manager.get()));
  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnerManagerTest, GetKeyFailDuringVerify) {
  StartUnowned();
  MockKeyLoadObserver loader(NULL);
  loader.ExpectKeyFetchSuccess(false);
  scoped_refptr<OwnerManager> manager(new OwnerManager);

  EXPECT_CALL(*mock_, GetOwnerKeyFilePath())
      .WillRepeatedly(Return(tmpfile_));
  EXPECT_CALL(*mock_, ImportPublicKey(tmpfile_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  base::WaitableEvent event(true, false);
  MockKeyUser delegate(OwnerManager::KEY_UNAVAILABLE, &event);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OwnerManager::Verify, manager.get(), BrowserThread::UI,
                 std::string(), std::vector<uint8>(), &delegate));
  while (!event.IsSignaled())
    message_loop_.RunAllPending();
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
  base::WaitableEvent event(true, false);
  MockKeyUser delegate(OwnerManager::SUCCESS, &event);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OwnerManager::Verify, manager.get(), BrowserThread::UI, data,
                 sig, &delegate));
  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

TEST_F(OwnerManagerTest, GetKeyAndVerify) {
  MockKeyLoadObserver loader(NULL);
  loader.ExpectKeyFetchSuccess(true);
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

  base::WaitableEvent event(true, false);
  MockKeyUser delegate(OwnerManager::SUCCESS, &event);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OwnerManager::Verify, manager.get(), BrowserThread::UI, data,
                 sig, &delegate));
  while (!event.IsSignaled())
    message_loop_.RunAllPending();
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
  base::WaitableEvent event(true, false);
  MockSigner delegate(OwnerManager::SUCCESS, sig, &event);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OwnerManager::Sign, manager.get(), BrowserThread::UI, data,
                 &delegate));
  while (!event.IsSignaled())
    message_loop_.RunAllPending();
}

}  // namespace chromeos
