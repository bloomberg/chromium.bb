// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/password_manager/password_store_win.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;
using testing::_;
using webkit_glue::PasswordForm;

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD2(OnPasswordStoreRequestDone,
               void(int, const std::vector<webkit_glue::PasswordForm*>&));
};

class MockWebDataServiceConsumer : public WebDataServiceConsumer {
public:
  MOCK_METHOD2(OnWebDataServiceRequestDone,
               void(WebDataService::Handle, const WDTypedResult*));
};

class SignalingTask : public Task {
 public:
  explicit SignalingTask(WaitableEvent* event) : event_(event) {
  }
  virtual void Run() {
    event_->Signal();
  }
 private:
  WaitableEvent* event_;
};

}  // anonymous namespace

class PasswordStoreWinTest : public testing::Test {
 protected:
  PasswordStoreWinTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        db_thread_(ChromeThread::DB) {
  }

  bool CreateIE7PasswordInfo(const std::wstring& url, const base::Time& created,
                             IE7PasswordInfo* info) {
    // Copied from chrome/browser/importer/importer_unittest.cc
    // The username is "abcdefgh" and the password "abcdefghijkl".
    unsigned char data[] = "\x0c\x00\x00\x00\x38\x00\x00\x00\x2c\x00\x00\x00"
                           "\x57\x49\x43\x4b\x18\x00\x00\x00\x02\x00\x00\x00"
                           "\x67\x00\x72\x00\x01\x00\x00\x00\x00\x00\x00\x00"
                           "\x00\x00\x00\x00\x4e\xfa\x67\x76\x22\x94\xc8\x01"
                           "\x08\x00\x00\x00\x12\x00\x00\x00\x4e\xfa\x67\x76"
                           "\x22\x94\xc8\x01\x0c\x00\x00\x00\x61\x00\x62\x00"
                           "\x63\x00\x64\x00\x65\x00\x66\x00\x67\x00\x68\x00"
                           "\x00\x00\x61\x00\x62\x00\x63\x00\x64\x00\x65\x00"
                           "\x66\x00\x67\x00\x68\x00\x69\x00\x6a\x00\x6b\x00"
                           "\x6c\x00\x00\x00";
    DATA_BLOB input = {0};
    DATA_BLOB url_key = {0};
    DATA_BLOB output = {0};

    input.pbData = data;
    input.cbData = sizeof(data);

    url_key.pbData = reinterpret_cast<unsigned char*>(
        const_cast<wchar_t*>(url.data()));
    url_key.cbData = static_cast<DWORD>((url.size() + 1) *
                                        sizeof(std::wstring::value_type));

    if (!CryptProtectData(&input, NULL, &url_key, NULL, NULL,
                          CRYPTPROTECT_UI_FORBIDDEN, &output))
      return false;

    std::vector<unsigned char> encrypted_data;
    encrypted_data.resize(output.cbData);
    memcpy(&encrypted_data.front(), output.pbData, output.cbData);

    LocalFree(output.pbData);

    info->url_hash = ie7_password::GetUrlHash(url);
    info->encrypted_data = encrypted_data;
    info->date_created = created;

    return true;
  }

  virtual void SetUp() {
    ASSERT_TRUE(db_thread_.Start());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    profile_.reset(new TestingProfile());

    login_db_.reset(new LoginDatabase());
    ASSERT_TRUE(login_db_->Init(temp_dir_.path().Append(
        FILE_PATH_LITERAL("login_test"))));

    wds_ = new WebDataService();
    ASSERT_TRUE(wds_->Init(temp_dir_.path()));
  }

  virtual void TearDown() {
    if (wds_.get())
      wds_->Shutdown();
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    MessageLoop::current()->Run();
    db_thread_.Stop();
  }

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  ChromeThread db_thread_;  // PasswordStore, WDS schedule work on this thread.

  scoped_ptr<LoginDatabase> login_db_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<WebDataService> wds_;
  ScopedTempDir temp_dir_;
};

ACTION(QuitUIMessageLoop) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  MessageLoop::current()->Quit();
}

TEST_F(PasswordStoreWinTest, ConvertIE7Login) {
  IE7PasswordInfo password_info;
  ASSERT_TRUE(CreateIE7PasswordInfo(L"http://example.com/origin",
                                    base::Time::FromDoubleT(1),
                                    &password_info));

  // This IE7 password will be retrieved by the GetLogins call.
  wds_->AddIE7Login(password_info);

  // The WDS schedules tasks to run on the DB thread so we schedule yet another
  // task to notify us that it's safe to carry on with the test.
  WaitableEvent done(false, false);
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, new SignalingTask(&done));
  done.Wait();

  // Prentend that the migration has already taken place.
  profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                            true);

  // Initializing the PasswordStore shouldn't trigger a migration.
  scoped_refptr<PasswordStoreWin> store(
      new PasswordStoreWin(login_db_.release(), profile_.get(), wds_.get()));
  EXPECT_TRUE(store->Init());

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnPasswordStoreRequestDone(_, _))
      .WillByDefault(QuitUIMessageLoop());

  PasswordFormData form_data = {
    PasswordForm::SCHEME_HTML,
    "http://example.com/",
    "http://example.com/origin",
    "http://example.com/action",
    L"submit_element",
    L"username_element",
    L"password_element",
    L"",
    L"",
    true, false, 1,
  };
  scoped_ptr<PasswordForm> form(CreatePasswordFormFromData(form_data));

  PasswordFormData expected_form_data = {
    PasswordForm::SCHEME_HTML,
    "http://example.com/",
    "http://example.com/origin",
    "http://example.com/action",
    L"submit_element",
    L"username_element",
    L"password_element",
    L"abcdefgh",
    L"abcdefghijkl",
    true, false, 1,
  };
  std::vector<PasswordForm*> forms;
  forms.push_back(CreatePasswordFormFromData(expected_form_data));

  // The IE7 password should be returned.
  EXPECT_CALL(consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(forms)))
      .WillOnce(QuitUIMessageLoop());

  store->GetLogins(*form, &consumer);
  MessageLoop::current()->Run();

  STLDeleteElements(&forms);
}

TEST_F(PasswordStoreWinTest, OutstandingWDSQueries) {
  // Prentend that the migration has already taken place.
  profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                            true);

  // Initializing the PasswordStore shouldn't trigger a migration.
  scoped_refptr<PasswordStoreWin> store(
      new PasswordStoreWin(login_db_.release(), profile_.get(), wds_.get()));
  EXPECT_TRUE(store->Init());

  PasswordFormData form_data = {
    PasswordForm::SCHEME_HTML,
    "http://example.com/",
    "http://example.com/origin",
    "http://example.com/action",
    L"submit_element",
    L"username_element",
    L"password_element",
    L"",
    L"",
    true, false, 1,
  };
  scoped_ptr<PasswordForm> form(CreatePasswordFormFromData(form_data));

  MockPasswordStoreConsumer consumer;
  store->GetLogins(*form, &consumer);

  // Release the PSW and the WDS before the query can return.
  store = NULL;
  wds_->Shutdown();
  wds_ = NULL;

  MessageLoop::current()->RunAllPending();
}

TEST_F(PasswordStoreWinTest, MultipleWDSQueriesOnDifferentThreads) {
  IE7PasswordInfo password_info;
  ASSERT_TRUE(CreateIE7PasswordInfo(L"http://example.com/origin",
                                    base::Time::FromDoubleT(1),
                                    &password_info));
  wds_->AddIE7Login(password_info);

  // The WDS schedules tasks to run on the DB thread so we schedule yet another
  // task to notify us that it's safe to carry on with the test.
  WaitableEvent done(false, false);
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, new SignalingTask(&done));
  done.Wait();

  // Prentend that the migration has already taken place.
  profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                            true);

  // Initializing the PasswordStore shouldn't trigger a migration.
  scoped_refptr<PasswordStoreWin> store(
      new PasswordStoreWin(login_db_.release(), profile_.get(), wds_.get()));
  EXPECT_TRUE(store->Init());

  MockPasswordStoreConsumer password_consumer;
  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(password_consumer, OnPasswordStoreRequestDone(_, _))
      .WillByDefault(QuitUIMessageLoop());

  PasswordFormData form_data = {
    PasswordForm::SCHEME_HTML,
    "http://example.com/",
    "http://example.com/origin",
    "http://example.com/action",
    L"submit_element",
    L"username_element",
    L"password_element",
    L"",
    L"",
    true, false, 1,
  };
  scoped_ptr<PasswordForm> form(CreatePasswordFormFromData(form_data));

  PasswordFormData expected_form_data = {
    PasswordForm::SCHEME_HTML,
    "http://example.com/",
    "http://example.com/origin",
    "http://example.com/action",
    L"submit_element",
    L"username_element",
    L"password_element",
    L"abcdefgh",
    L"abcdefghijkl",
    true, false, 1,
  };
  std::vector<PasswordForm*> forms;
  forms.push_back(CreatePasswordFormFromData(expected_form_data));

  // The IE7 password should be returned.
  EXPECT_CALL(password_consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(forms)))
      .WillOnce(QuitUIMessageLoop());

  store->GetLogins(*form, &password_consumer);

  MockWebDataServiceConsumer wds_consumer;

  EXPECT_CALL(wds_consumer,
    OnWebDataServiceRequestDone(_, _))
    .WillOnce(QuitUIMessageLoop());

  wds_->GetIE7Login(password_info, &wds_consumer);

  // Run the MessageLoop twice: once for the GetIE7Login that PasswordStoreWin
  // schedules on the DB thread and once for the one we just scheduled on the UI
  // thread.
  MessageLoop::current()->Run();
  MessageLoop::current()->Run();

  STLDeleteElements(&forms);
}