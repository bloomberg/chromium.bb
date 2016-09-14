// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/simple_password_store_mac.h"

#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/login_database.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using password_manager::PasswordStoreChange;
using testing::ElementsAreArray;

namespace {

class MockPasswordStoreObserver
    : public password_manager::PasswordStore::Observer {
 public:
  MOCK_METHOD1(OnLoginsChanged,
               void(const password_manager::PasswordStoreChangeList& changes));
};

class SimplePasswordStoreMacTest : public testing::Test {
 public:
  SimplePasswordStoreMacTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_FILE_THREAD) {}

  void SetUp() override {
    ASSERT_TRUE(db_dir_.CreateUniqueTempDir());

    // Ensure that LoginDatabase will use the mock keychain if it needs to
    // encrypt/decrypt a password.
    OSCryptMocker::SetUpWithSingleton();

    // Setup LoginDatabase.
    std::unique_ptr<password_manager::LoginDatabase> login_db(
        new password_manager::LoginDatabase(test_login_db_file_path()));
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner =
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE);
    ASSERT_TRUE(file_task_runner);
    file_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&InitOnBackgroundThread, base::Unretained(login_db.get())));
    store_ = new SimplePasswordStoreMac(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::UI), nullptr,
        std::unique_ptr<password_manager::LoginDatabase>());
    file_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&SimplePasswordStoreMac::InitWithTaskRunner, store_,
                   file_task_runner, base::Passed(&login_db)));
    // Make sure deferred initialization is finished.
    FinishAsyncProcessing();
  }

  void TearDown() override {
    store_->ShutdownOnUIThread();
    EXPECT_TRUE(store_->GetBackgroundTaskRunner());
    store_ = nullptr;
    OSCryptMocker::TearDown();
  }

  base::FilePath test_login_db_file_path() const {
    return db_dir_.GetPath().Append(FILE_PATH_LITERAL("login.db"));
  }

  scoped_refptr<SimplePasswordStoreMac> store() { return store_; }

  void FinishAsyncProcessing() {
    scoped_refptr<content::MessageLoopRunner> runner(
        new content::MessageLoopRunner);
    ASSERT_TRUE(BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
                                                base::Bind(&Noop),
                                                runner->QuitClosure()));
    runner->Run();
  }

 private:
  static void InitOnBackgroundThread(password_manager::LoginDatabase* db) {
    ASSERT_TRUE(db->Init());
  }

  static void Noop() {}

  base::ScopedTempDir db_dir_;
  scoped_refptr<SimplePasswordStoreMac> store_;

  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(SimplePasswordStoreMacTest, Sanity) {
  autofill::PasswordForm form;
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = base::ASCIIToUTF16("my_username");
  form.password_value = base::ASCIIToUTF16("12345");
  MockPasswordStoreObserver observer;
  store()->AddObserver(&observer);

  const PasswordStoreChange expected_add_changes[] = {
      PasswordStoreChange(PasswordStoreChange::ADD, form),
  };
  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_add_changes)));
  // Adding a login should trigger a notification.
  store()->AddLogin(form);
  FinishAsyncProcessing();
}

}  // namespace
