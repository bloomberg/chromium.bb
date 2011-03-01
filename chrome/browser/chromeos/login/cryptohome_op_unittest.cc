// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/cryptohome_op.h"

#include <string>

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/mock_auth_attempt_state_resolver.h"
#include "chrome/browser/chromeos/login/test_attempt_state.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace chromeos {

class CryptohomeOpTest : public ::testing::Test {
 public:
  CryptohomeOpTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO),
        username_("me@nowhere.org"),
        hash_ascii_("0a010000000000a0"),
        state_(username_, "", hash_ascii_, "", "", false),
        resolver_(new MockAuthAttemptStateResolver),
        mock_library_(new MockCryptohomeLibrary) {
  }

  virtual ~CryptohomeOpTest() {}

  virtual void SetUp() {
    CrosLibrary::TestApi* test_api = CrosLibrary::Get()->GetTestApi();

    MockLibraryLoader* loader = new MockLibraryLoader();
    ON_CALL(*loader, Load(_))
        .WillByDefault(Return(true));
    EXPECT_CALL(*loader, Load(_))
        .Times(AnyNumber());

    // Passes ownership of |loader| to CrosLibrary.
    test_api->SetLibraryLoader(loader, true);
    // |mock_library_| is mine, though.
    test_api->SetCryptohomeLibrary(mock_library_.get(), false);
    mock_library_->SetUp(false, 0);

    io_thread_.Start();
  }

  virtual void TearDown() {
    // Prevent bogus gMock leak check from firing.
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();
    test_api->SetLibraryLoader(NULL, false);
  }

  void ExpectMigrate(bool passing_old_hash, const std::string& hash) {
    if (passing_old_hash) {
      EXPECT_CALL(*(mock_library_.get()), AsyncMigrateKey(username_,
                                                          hash,
                                                          hash_ascii_,
                                                          _))
          .Times(1)
          .RetiresOnSaturation();
    } else {
      EXPECT_CALL(*(mock_library_.get()), AsyncMigrateKey(username_,
                                                          hash_ascii_,
                                                          hash,
                                                          _))
          .Times(1)
          .RetiresOnSaturation();
    }
  }

  void ExpectMount() {
    EXPECT_CALL(*(mock_library_.get()),
                AsyncMount(username_, hash_ascii_, true, _))
        .Times(1)
        .RetiresOnSaturation();
  }

  void ExpectMountGuest() {
    EXPECT_CALL(*(mock_library_.get()), AsyncMountForBwsi(_))
        .Times(1)
        .RetiresOnSaturation();
  }

  void ExpectRemove() {
    EXPECT_CALL(*(mock_library_.get()), AsyncRemove(username_, _))
        .Times(1)
        .RetiresOnSaturation();
  }

  void ExpectCheckKey() {
    EXPECT_CALL(*(mock_library_.get()),
                AsyncCheckKey(username_, hash_ascii_, _))
        .Times(1)
        .RetiresOnSaturation();
  }

  void RunTest(CryptohomeOp* op, bool outcome, int code) {
    mock_library_->SetAsyncBehavior(outcome, code);

    EXPECT_CALL(*(resolver_.get()), Resolve())
        .Times(1)
        .RetiresOnSaturation();

    EXPECT_TRUE(op->Initiate());
    // Force IO thread to finish tasks so I can verify |state_|.
    io_thread_.Stop();

    EXPECT_EQ(outcome, state_.cryptohome_outcome());
    EXPECT_EQ(code, state_.cryptohome_code());
  }

  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
  std::string username_;
  std::string hash_ascii_;
  TestAttemptState state_;
  scoped_ptr<MockAuthAttemptStateResolver> resolver_;
  scoped_refptr<CryptohomeOp> op_;
  scoped_ptr<MockCryptohomeLibrary> mock_library_;

};

TEST_F(CryptohomeOpTest, MountSuccess) {
  ExpectMount();
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateMountAttempt(&state_, resolver_.get(), true));
  RunTest(op.get(), true, kCryptohomeMountErrorNone);
}

TEST_F(CryptohomeOpTest, MountFatal) {
  ExpectMount();
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateMountAttempt(&state_, resolver_.get(), true));
  RunTest(op.get(), false, kCryptohomeMountErrorFatal);
}

TEST_F(CryptohomeOpTest, MountKeyFailure) {
  ExpectMount();
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateMountAttempt(&state_, resolver_.get(), true));
  RunTest(op.get(), false, kCryptohomeMountErrorKeyFailure);
}

TEST_F(CryptohomeOpTest, MountRecreated) {
  ExpectMount();
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateMountAttempt(&state_, resolver_.get(), true));
  RunTest(op.get(), true, kCryptohomeMountErrorRecreated);
}

TEST_F(CryptohomeOpTest, MountGuestSuccess) {
  ExpectMountGuest();
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateMountGuestAttempt(&state_, resolver_.get()));
  RunTest(op.get(), true, kCryptohomeMountErrorNone);
}

TEST_F(CryptohomeOpTest, MountGuestFatal) {
  ExpectMountGuest();
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateMountGuestAttempt(&state_, resolver_.get()));
  RunTest(op.get(), false, kCryptohomeMountErrorFatal);
}

TEST_F(CryptohomeOpTest, MigrateSuccessPassOld) {
  ExpectMigrate(true, "");
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateMigrateAttempt(&state_, resolver_.get(), true, ""));
  RunTest(op.get(), true, kCryptohomeMountErrorNone);
}

TEST_F(CryptohomeOpTest, MigrateSuccessPassNew) {
  ExpectMigrate(false, "");
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateMigrateAttempt(&state_, resolver_.get(), false, ""));
  RunTest(op.get(), true, kCryptohomeMountErrorNone);
}

TEST_F(CryptohomeOpTest, MigrateKeyFailure) {
  ExpectMigrate(true, "");
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateMigrateAttempt(&state_, resolver_.get(), true, ""));
  RunTest(op.get(), false, kCryptohomeMountErrorKeyFailure);
}

TEST_F(CryptohomeOpTest, RemoveSuccess) {
  ExpectRemove();
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateRemoveAttempt(&state_, resolver_.get()));
  RunTest(op.get(), true, kCryptohomeMountErrorNone);
}

TEST_F(CryptohomeOpTest, RemoveFailure) {
  ExpectRemove();
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateRemoveAttempt(&state_, resolver_.get()));
  RunTest(op.get(), false, kCryptohomeMountErrorKeyFailure);
}

TEST_F(CryptohomeOpTest, CheckKeySuccess) {
  ExpectCheckKey();
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateCheckKeyAttempt(&state_, resolver_.get()));
  RunTest(op.get(), true, kCryptohomeMountErrorNone);
}

TEST_F(CryptohomeOpTest, CheckKeyFailure) {
  ExpectCheckKey();
  scoped_refptr<CryptohomeOp> op(
      CryptohomeOp::CreateCheckKeyAttempt(&state_, resolver_.get()));
  RunTest(op.get(), false, kCryptohomeMountErrorKeyFailure);
}

}  // namespace chromeos
