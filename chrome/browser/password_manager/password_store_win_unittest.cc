// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/password_manager/password_store_consumer.h"
#include "chrome/browser/password_manager/password_store_win.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;
using content::BrowserThread;
using testing::_;
using testing::DoAll;
using testing::WithArg;
using content::PasswordForm;

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD2(OnPasswordStoreRequestDone,
               void(CancelableRequestProvider::Handle,
                    const std::vector<content::PasswordForm*>&));
  MOCK_METHOD1(OnGetPasswordStoreResults,
               void(const std::vector<content::PasswordForm*>&));
};

class MockWebDataServiceConsumer : public WebDataServiceConsumer {
public:
  MOCK_METHOD2(OnWebDataServiceRequestDone,
               void(WebDataService::Handle, const WDTypedResult*));
};

}  // anonymous namespace

typedef std::vector<PasswordForm*> VectorOfForms;

class PasswordStoreWinTest : public testing::Test {
 protected:
  PasswordStoreWinTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {
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
    wds_ = new WebDataService(WebDataServiceBase::ProfileErrorCallback());
    base::FilePath path = temp_dir_.path().AppendASCII("web_data_test");
    wds_->Init(path);
  }

  virtual void TearDown() {
    if (store_.get())
      store_->ShutdownOnUIThread();
    wds_->ShutdownOnUIThread();
    wds_ = NULL;
    base::WaitableEvent done(false, false);
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
    db_thread_.Stop();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  // PasswordStore, WDS schedule work on this thread.
  content::TestBrowserThread db_thread_;

  scoped_ptr<LoginDatabase> login_db_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<WebDataService> wds_;
  scoped_refptr<PasswordStore> store_;
  base::ScopedTempDir temp_dir_;
};

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

ACTION(QuitUIMessageLoop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoop::current()->Quit();
}

MATCHER(EmptyWDResult, "") {
  return static_cast<const WDResult<std::vector<PasswordForm*> >*>(
      arg)->GetValue().empty();
}

// Hangs flakily, http://crbug.com/71385.
TEST_F(PasswordStoreWinTest, DISABLED_ConvertIE7Login) {
  IE7PasswordInfo password_info;
  ASSERT_TRUE(CreateIE7PasswordInfo(L"http://example.com/origin",
                                    base::Time::FromDoubleT(1),
                                    &password_info));
  // Verify the URL hash
  ASSERT_EQ(L"39471418FF5453FEEB3731E382DEB5D53E14FAF9B5",
            password_info.url_hash);

  // This IE7 password will be retrieved by the GetLogins call.
  wds_->AddIE7Login(password_info);

  // The WDS schedules tasks to run on the DB thread so we schedule yet another
  // task to notify us that it's safe to carry on with the test.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();

  store_ = new PasswordStoreWin(login_db_.release(), profile_.get(),
                                wds_.get());
  EXPECT_TRUE(store_->Init());

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnGetPasswordStoreResults(_))
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
              OnGetPasswordStoreResults(ContainsAllPasswordForms(forms)))
      .WillOnce(QuitUIMessageLoop());

  store_->GetLogins(*form, &consumer);
  MessageLoop::current()->Run();

  STLDeleteElements(&forms);
}

// Crashy.  http://crbug.com/86558
TEST_F(PasswordStoreWinTest, DISABLED_OutstandingWDSQueries) {
  store_ = new PasswordStoreWin(login_db_.release(), profile_.get(),
                                wds_.get());
  EXPECT_TRUE(store_->Init());

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
  store_->GetLogins(*form, &consumer);

  // Release the PSW and the WDS before the query can return.
  store_->ShutdownOnUIThread();
  store_ = NULL;
  wds_ = NULL;

  MessageLoop::current()->RunUntilIdle();
}

// Hangs flakily, see http://crbug.com/43836.
TEST_F(PasswordStoreWinTest, DISABLED_MultipleWDSQueriesOnDifferentThreads) {
  IE7PasswordInfo password_info;
  ASSERT_TRUE(CreateIE7PasswordInfo(L"http://example.com/origin",
                                    base::Time::FromDoubleT(1),
                                    &password_info));
  wds_->AddIE7Login(password_info);

  // The WDS schedules tasks to run on the DB thread so we schedule yet another
  // task to notify us that it's safe to carry on with the test.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();

  store_ = new PasswordStoreWin(login_db_.release(), profile_.get(),
                                wds_.get());
  EXPECT_TRUE(store_->Init());

  MockPasswordStoreConsumer password_consumer;
  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(password_consumer, OnGetPasswordStoreResults(_))
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
              OnGetPasswordStoreResults(ContainsAllPasswordForms(forms)))
      .WillOnce(QuitUIMessageLoop());

  store_->GetLogins(*form, &password_consumer);

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

TEST_F(PasswordStoreWinTest, EmptyLogins) {
  store_ = new PasswordStoreWin(login_db_.release(), profile_.get(),
                                wds_.get());
  store_->Init();

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

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnGetPasswordStoreResults(_))
      .WillByDefault(QuitUIMessageLoop());

  VectorOfForms expect_none;
  // expect that we get no results;
  EXPECT_CALL(consumer,
              OnGetPasswordStoreResults(ContainsAllPasswordForms(expect_none)))
      .WillOnce(DoAll(WithArg<0>(STLDeleteElements0()), QuitUIMessageLoop()));

  store_->GetLogins(*form, &consumer);
  MessageLoop::current()->Run();
}

TEST_F(PasswordStoreWinTest, EmptyBlacklistLogins) {
  store_ = new PasswordStoreWin(login_db_.release(), profile_.get(),
                                wds_.get());
  store_->Init();

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnPasswordStoreRequestDone(_, _))
      .WillByDefault(QuitUIMessageLoop());

  VectorOfForms expect_none;
  // expect that we get no results;
  EXPECT_CALL(
      consumer,
      OnPasswordStoreRequestDone(_, ContainsAllPasswordForms(expect_none)))
      .WillOnce(DoAll(WithArg<1>(STLDeleteElements0()), QuitUIMessageLoop()));

  store_->GetBlacklistLogins(&consumer);
  MessageLoop::current()->Run();
}

TEST_F(PasswordStoreWinTest, EmptyAutofillableLogins) {
  store_ = new PasswordStoreWin(login_db_.release(), profile_.get(),
                                wds_.get());
  store_->Init();

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnPasswordStoreRequestDone(_, _))
      .WillByDefault(QuitUIMessageLoop());

  VectorOfForms expect_none;
  // expect that we get no results;
  EXPECT_CALL(
      consumer,
      OnPasswordStoreRequestDone(_, ContainsAllPasswordForms(expect_none)))
      .WillOnce(DoAll(WithArg<1>(STLDeleteElements0()), QuitUIMessageLoop()));

  store_->GetAutofillableLogins(&consumer);
  MessageLoop::current()->Run();
}
