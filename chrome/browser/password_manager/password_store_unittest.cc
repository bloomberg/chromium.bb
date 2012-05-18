// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/password_manager/password_store_consumer.h"
#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/test/notification_observer_mock.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;
using content::BrowserThread;
using testing::_;
using testing::DoAll;
using testing::WithArg;
using webkit::forms::PasswordForm;

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD2(OnPasswordStoreRequestDone,
               void(CancelableRequestProvider::Handle,
                    const std::vector<PasswordForm*>&));
};

// This class will add and remove a mock notification observer from
// the DB thread.
class DBThreadObserverHelper
    : public base::RefCountedThreadSafe<DBThreadObserverHelper,
                                        BrowserThread::DeleteOnDBThread> {
 public:
  DBThreadObserverHelper() : done_event_(true, false) {}

  void Init(PasswordStore* password_store) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(&DBThreadObserverHelper::AddObserverTask,
                   this,
                   make_scoped_refptr(password_store)));
    done_event_.Wait();
  }

  content::NotificationObserverMock& observer() {
    return observer_;
  }

 protected:
  virtual ~DBThreadObserverHelper() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    registrar_.RemoveAll();
  }

  void AddObserverTask(PasswordStore* password_store) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    registrar_.Add(&observer_,
                   chrome::NOTIFICATION_LOGINS_CHANGED,
                   content::Source<PasswordStore>(password_store));
    done_event_.Signal();
  }

  WaitableEvent done_event_;
  content::NotificationRegistrar registrar_;
  content::NotificationObserverMock observer_;

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::DB>;
  friend class base::DeleteHelper<DBThreadObserverHelper>;
};

}  // anonymous namespace

class PasswordStoreTest : public testing::Test {
 protected:
  PasswordStoreTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(db_thread_.Start());

    profile_.reset(new TestingProfile());

    login_db_.reset(new LoginDatabase());
    ASSERT_TRUE(login_db_->Init(profile_->GetPath().Append(
        FILE_PATH_LITERAL("login_test"))));
  }

  virtual void TearDown() {
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
    db_thread_.Stop();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  // PasswordStore schedules work on this thread.
  content::TestBrowserThread db_thread_;

  scoped_ptr<LoginDatabase> login_db_;
  scoped_ptr<TestingProfile> profile_;
};

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

ACTION(QuitUIMessageLoop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoop::current()->Quit();
}

TEST_F(PasswordStoreTest, IgnoreOldWwwGoogleLogins) {
  scoped_refptr<PasswordStoreDefault> store(
      new PasswordStoreDefault(login_db_.release(), profile_.get()));
  store->Init();

  const time_t cutoff = 1325376000;  // 00:00 Jan 1 2012 UTC
  // The passwords are all empty because PasswordStoreDefault doesn't store the
  // actual passwords on OS X (they're stored in the Keychain instead). We could
  // special-case it, but it's easier to just have empty passwords.
  static const PasswordFormData form_data[] = {
    // A form on https://www.google.com/ older than the cutoff. Will be ignored.
    { PasswordForm::SCHEME_HTML,
      "https://www.google.com",
      "https://www.google.com/origin",
      "https://www.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value_1",
      L"",
      true, true, cutoff - 1 },
    // A form on https://www.google.com/ older than the cutoff. Will be ignored.
    { PasswordForm::SCHEME_HTML,
      "https://www.google.com/",
      "https://www.google.com/origin",
      "https://www.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value_2",
      L"",
      true, true, cutoff - 1 },
    // A form on https://www.google.com/ newer than the cutoff.
    { PasswordForm::SCHEME_HTML,
      "https://www.google.com",
      "https://www.google.com/origin",
      "https://www.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value_3",
      L"",
      true, true, cutoff + 1 },
    // A form on https://accounts.google.com/ older than the cutoff.
    { PasswordForm::SCHEME_HTML,
      "https://accounts.google.com",
      "https://accounts.google.com/origin",
      "https://accounts.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"",
      true, true, cutoff - 1 },
    // A form on http://bar.example.com/ older than the cutoff.
    { PasswordForm::SCHEME_HTML,
      "http://bar.example.com",
      "http://bar.example.com/origin",
      "http://bar.example.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"",
      true, false, cutoff - 1 },
  };

  // Build the forms vector and add the forms to the store.
  std::vector<PasswordForm*> all_forms;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(form_data); ++i) {
    PasswordForm* form = CreatePasswordFormFromData(form_data[i]);
    all_forms.push_back(form);
    store->AddLogin(*form);
  }

  // The PasswordStore schedules tasks to run on the DB thread so we schedule
  // yet another task to notify us that it's safe to carry on with the test.
  // The PasswordStore doesn't really understand that it's "done" once the tasks
  // we posted above have completed, so there's no formal notification for that.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();

  // We expect to get back only the "recent" www.google.com login.
  // Theoretically these should never actually exist since there are no longer
  // any login forms on www.google.com to save, but we technically allow them.
  // We should not get back the older saved password though.
  PasswordForm www_google;
  www_google.scheme = PasswordForm::SCHEME_HTML;
  www_google.signon_realm = "https://www.google.com";
  std::vector<PasswordForm*> www_google_expected;
  www_google_expected.push_back(all_forms[2]);

  // We should still get the accounts.google.com login even though it's older
  // than our cutoff - this is the new location of all Google login forms.
  PasswordForm accounts_google;
  accounts_google.scheme = PasswordForm::SCHEME_HTML;
  accounts_google.signon_realm = "https://accounts.google.com";
  std::vector<PasswordForm*> accounts_google_expected;
  accounts_google_expected.push_back(all_forms[3]);

  // Same thing for a generic saved login.
  PasswordForm bar_example;
  bar_example.scheme = PasswordForm::SCHEME_HTML;
  bar_example.signon_realm = "http://bar.example.com";
  std::vector<PasswordForm*> bar_example_expected;
  bar_example_expected.push_back(all_forms[4]);

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnPasswordStoreRequestDone(_, _))
      .WillByDefault(QuitUIMessageLoop());

  // Expect the appropriate replies, as above, in reverse order than we will
  // issue the queries. Each retires on saturation to avoid matcher spew, except
  // the last which quits the message loop.
  EXPECT_CALL(consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(bar_example_expected)))
      .WillOnce(DoAll(WithArg<1>(STLDeleteElements0()), QuitUIMessageLoop()));
  EXPECT_CALL(consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(accounts_google_expected)))
      .WillOnce(WithArg<1>(STLDeleteElements0())).RetiresOnSaturation();
  EXPECT_CALL(consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(www_google_expected)))
      .WillOnce(WithArg<1>(STLDeleteElements0())).RetiresOnSaturation();

  store->GetLogins(www_google, &consumer);
  store->GetLogins(accounts_google, &consumer);
  store->GetLogins(bar_example, &consumer);

  MessageLoop::current()->Run();

  STLDeleteElements(&all_forms);
}
