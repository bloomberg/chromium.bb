// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_win.h"

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/os_crypt/ie7_password_win.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/webdata/logins_table.h"
#include "components/password_manager/core/browser/webdata/password_web_data_service_win.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webdata/common/web_database_service.h"
#include "content/public/test/test_browser_thread.h"
#include "crypto/wincrypt_shim.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::WaitableEvent;
using content::BrowserThread;
using password_manager::LoginDatabase;
using password_manager::PasswordFormData;
using password_manager::PasswordStore;
using password_manager::PasswordStoreConsumer;
using password_manager::UnorderedPasswordFormElementsAre;
using testing::_;
using testing::DoAll;
using testing::IsEmpty;
using testing::WithArg;

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResultsConstRef,
               void(const std::vector<std::unique_ptr<PasswordForm>>&));

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    OnGetPasswordStoreResultsConstRef(results);
  }
};

class MockWebDataServiceConsumer : public WebDataServiceConsumer {
 public:
  MOCK_METHOD0(OnWebDataServiceRequestDoneStub, void());

  // GMock cannot mock methods with move-only args.
  void OnWebDataServiceRequestDone(WebDataServiceBase::Handle h,
                                   std::unique_ptr<WDTypedResult> result) {
    OnWebDataServiceRequestDoneStub();
  }
};

}  // anonymous namespace

class PasswordStoreWinTest : public testing::Test {
 protected:
  PasswordStoreWinTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {}

  bool CreateIE7PasswordInfo(const std::wstring& url,
                             const base::Time& created,
                             IE7PasswordInfo* info) {
    // Copied from chrome/browser/importer/importer_unittest.cc
    // The username is "abcdefgh" and the password "abcdefghijkl".
    unsigned char data[] =
        "\x0c\x00\x00\x00\x38\x00\x00\x00\x2c\x00\x00\x00"
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

    url_key.pbData =
        reinterpret_cast<unsigned char*>(const_cast<wchar_t*>(url.data()));
    url_key.cbData =
        static_cast<DWORD>((url.size() + 1) * sizeof(std::wstring::value_type));

    if (!CryptProtectData(&input, nullptr, &url_key, nullptr, nullptr,
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

  void SetUp() override {
    ASSERT_TRUE(db_thread_.Start());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    profile_.reset(new TestingProfile());

    base::FilePath path = temp_dir_.GetPath().AppendASCII("web_data_test");
    wdbs_ = new WebDatabaseService(
        path, BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
        BrowserThread::GetTaskRunnerForThread(BrowserThread::DB));
    // Need to add at least one table so the database gets created.
    wdbs_->AddTable(std::unique_ptr<WebDatabaseTable>(new LoginsTable()));
    wdbs_->LoadDatabase();
    wds_ = new PasswordWebDataService(
        wdbs_, BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
        WebDataServiceBase::ProfileErrorCallback());
    wds_->Init();
  }

  void TearDown() override {
    if (store_.get())
      store_->ShutdownOnUIThread();
    if (wds_) {
      wds_->ShutdownOnUIThread();
      wds_ = nullptr;
    }
    if (wdbs_) {
      wdbs_->ShutdownDatabase();
      wdbs_ = nullptr;
    }
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    base::RunLoop().Run();
    db_thread_.Stop();
  }

  base::FilePath test_login_db_file_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
  }

  PasswordStoreWin* CreatePasswordStore() {
    return new PasswordStoreWin(
        base::ThreadTaskRunnerHandle::Get(),
        BrowserThread::GetTaskRunnerForThread(BrowserThread::DB),
        base::MakeUnique<LoginDatabase>(test_login_db_file_path()), wds_.get());
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  // PasswordStore, WDS schedule work on this thread.
  content::TestBrowserThread db_thread_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<PasswordWebDataService> wds_;
  scoped_refptr<WebDatabaseService> wdbs_;
  scoped_refptr<PasswordStore> store_;
};

ACTION(QuitUIMessageLoop) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::MessageLoop::current()->QuitWhenIdle();
}

MATCHER(EmptyWDResult, "") {
  return static_cast<
             const WDResult<std::vector<std::unique_ptr<PasswordForm>>>*>(arg)
      ->GetValue()
      .empty();
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
  WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED);
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();

  store_ = CreatePasswordStore();
  EXPECT_TRUE(store_->Init(syncer::SyncableService::StartSyncFlare()));

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnGetPasswordStoreResultsConstRef(_))
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
      true,
      1,
  };
  PasswordStore::FormDigest form(
      *CreatePasswordFormFromDataForTesting(form_data));

  // The returned form will not have 'action' or '*_element' fields set. This
  // is because credentials imported from IE don't have this information.
  PasswordFormData expected_form_data = {
      PasswordForm::SCHEME_HTML,
      "http://example.com/",
      "http://example.com/origin",
      "",
      L"",
      L"",
      L"",
      L"abcdefgh",
      L"abcdefghijkl",
      true,
      1,
  };
  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  expected_forms.push_back(
      CreatePasswordFormFromDataForTesting(expected_form_data));

  // The IE7 password should be returned.
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(
                            UnorderedPasswordFormElementsAre(&expected_forms)));

  store_->GetLogins(form, &consumer);
  base::RunLoop().Run();
}

TEST_F(PasswordStoreWinTest, OutstandingWDSQueries) {
  store_ = CreatePasswordStore();
  EXPECT_TRUE(store_->Init(syncer::SyncableService::StartSyncFlare()));

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
      true,
      1,
  };
  PasswordStore::FormDigest form(
      *CreatePasswordFormFromDataForTesting(form_data));

  MockPasswordStoreConsumer consumer;
  store_->GetLogins(form, &consumer);

  // Release the PSW and the WDS before the query can return.
  store_->ShutdownOnUIThread();
  store_ = nullptr;
  wds_->ShutdownOnUIThread();
  wds_ = nullptr;
  wdbs_->ShutdownDatabase();
  wdbs_ = nullptr;

  base::RunLoop().RunUntilIdle();
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
  WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED);
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();

  store_ = CreatePasswordStore();
  EXPECT_TRUE(store_->Init(syncer::SyncableService::StartSyncFlare()));

  MockPasswordStoreConsumer password_consumer;
  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(password_consumer, OnGetPasswordStoreResultsConstRef(_))
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
      true,
      1,
  };
  PasswordStore::FormDigest form(
      *CreatePasswordFormFromDataForTesting(form_data));

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
      true,
      1,
  };
  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  expected_forms.push_back(
      CreatePasswordFormFromDataForTesting(expected_form_data));

  // The IE7 password should be returned.
  EXPECT_CALL(password_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(&expected_forms)));

  store_->GetLogins(form, &password_consumer);

  MockWebDataServiceConsumer wds_consumer;

  EXPECT_CALL(wds_consumer, OnWebDataServiceRequestDoneStub())
      .WillOnce(QuitUIMessageLoop());

  wds_->GetIE7Login(password_info, &wds_consumer);

  // Run the MessageLoop twice: once for the GetIE7Login that PasswordStoreWin
  // schedules on the DB thread and once for the one we just scheduled on the UI
  // thread.
  base::RunLoop().Run();
  base::RunLoop().Run();
}

TEST_F(PasswordStoreWinTest, EmptyLogins) {
  store_ = CreatePasswordStore();
  store_->Init(syncer::SyncableService::StartSyncFlare());

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
      true,
      1,
  };
  PasswordStore::FormDigest form(
      *CreatePasswordFormFromDataForTesting(form_data));

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnGetPasswordStoreResultsConstRef(_))
      .WillByDefault(QuitUIMessageLoop());

  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));

  store_->GetLogins(form, &consumer);
  base::RunLoop().Run();
}

TEST_F(PasswordStoreWinTest, EmptyBlacklistLogins) {
  store_ = CreatePasswordStore();
  store_->Init(syncer::SyncableService::StartSyncFlare());

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnGetPasswordStoreResultsConstRef(_))
      .WillByDefault(QuitUIMessageLoop());

  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));

  store_->GetBlacklistLogins(&consumer);
  base::RunLoop().Run();
}

TEST_F(PasswordStoreWinTest, EmptyAutofillableLogins) {
  store_ = CreatePasswordStore();
  store_->Init(syncer::SyncableService::StartSyncFlare());

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnGetPasswordStoreResultsConstRef(_))
      .WillByDefault(QuitUIMessageLoop());

  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));

  store_->GetAutofillableLogins(&consumer);
  base::RunLoop().Run();
}
