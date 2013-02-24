// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "chrome/browser/password_manager/password_store_consumer.h"
#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/mock_notification_observer.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;
using content::BrowserThread;
using content::PasswordForm;
using testing::_;
using testing::DoAll;
using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;
using testing::WithArg;

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD2(OnPasswordStoreRequestDone,
               void(CancelableRequestProvider::Handle,
                    const std::vector<PasswordForm*>&));
  MOCK_METHOD1(OnGetPasswordStoreResults,
               void(const std::vector<PasswordForm*>&));
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

  content::MockNotificationObserver& observer() {
    return observer_;
  }

 protected:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::DB>;
  friend class base::DeleteHelper<DBThreadObserverHelper>;

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
  content::MockNotificationObserver observer_;
};

}  // anonymous namespace

class PasswordStoreDefaultTest : public testing::Test {
 protected:
  PasswordStoreDefaultTest()
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
  // PasswordStore, WDS schedule work on this thread.
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

TEST_F(PasswordStoreDefaultTest, NonASCIIData) {
  scoped_refptr<PasswordStoreDefault> store(
      new PasswordStoreDefault(login_db_.release(), profile_.get()));
  store->Init();

  // Some non-ASCII password form data.
  static const PasswordFormData form_data[] = {
    { PasswordForm::SCHEME_HTML,
      "http://foo.example.com",
      "http://foo.example.com/origin",
      "http://foo.example.com/action",
      L"มีสีสัน",
      L"お元気ですか?",
      L"盆栽",
      L"أحب كرة",
      L"£éä국수çà",
      true, false, 1 },
  };

  // Build the expected forms vector and add the forms to the store.
  std::vector<PasswordForm*> expected_forms;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(form_data); ++i) {
    PasswordForm* form = CreatePasswordFormFromData(form_data[i]);
    expected_forms.push_back(form);
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

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnPasswordStoreRequestDone(_, _))
      .WillByDefault(QuitUIMessageLoop());

  // We expect to get the same data back, even though it's not all ASCII.
  EXPECT_CALL(consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(expected_forms)))
      .WillOnce(DoAll(WithArg<1>(STLDeleteElements0()), QuitUIMessageLoop()));

  store->GetAutofillableLogins(&consumer);
  MessageLoop::current()->Run();

  STLDeleteElements(&expected_forms);
}

TEST_F(PasswordStoreDefaultTest, Notifications) {
  scoped_refptr<PasswordStore> store(
      new PasswordStoreDefault(login_db_.release(), profile_.get()));
  store->Init();

  PasswordFormData form_data =
  { PasswordForm::SCHEME_HTML,
    "http://bar.example.com",
    "http://bar.example.com/origin",
    "http://bar.example.com/action",
    L"submit_element",
    L"username_element",
    L"password_element",
    L"username_value",
    L"password_value",
    true, false, 1 };
  scoped_ptr<PasswordForm> form(CreatePasswordFormFromData(form_data));

  scoped_refptr<DBThreadObserverHelper> helper = new DBThreadObserverHelper;
  helper->Init(store);

  const PasswordStoreChange expected_add_changes[] = {
    PasswordStoreChange(PasswordStoreChange::ADD, *form),
  };

  EXPECT_CALL(helper->observer(),
      Observe(int(chrome::NOTIFICATION_LOGINS_CHANGED),
          content::Source<PasswordStore>(store),
          Property(&content::Details<const PasswordStoreChangeList>::ptr,
                   Pointee(ElementsAreArray(
                       expected_add_changes)))));

  // Adding a login should trigger a notification.
  store->AddLogin(*form);

  // The PasswordStore schedules tasks to run on the DB thread so we schedule
  // yet another task to notify us that it's safe to carry on with the test.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();

  // Change the password.
  form->password_value = base::WideToUTF16(L"a different password");

  const PasswordStoreChange expected_update_changes[] = {
    PasswordStoreChange(PasswordStoreChange::UPDATE, *form),
  };

  EXPECT_CALL(helper->observer(),
      Observe(int(chrome::NOTIFICATION_LOGINS_CHANGED),
              content::Source<PasswordStore>(store),
              Property(&content::Details<const PasswordStoreChangeList>::ptr,
                       Pointee(ElementsAreArray(
                           expected_update_changes)))));

  // Updating the login with the new password should trigger a notification.
  store->UpdateLogin(*form);

  // Wait for PasswordStore to send the notification.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();

  const PasswordStoreChange expected_delete_changes[] = {
    PasswordStoreChange(PasswordStoreChange::REMOVE, *form),
  };

  EXPECT_CALL(helper->observer(),
      Observe(int(chrome::NOTIFICATION_LOGINS_CHANGED),
              content::Source<PasswordStore>(store),
              Property(&content::Details<const PasswordStoreChangeList>::ptr,
                       Pointee(ElementsAreArray(
                           expected_delete_changes)))));

  // Deleting the login should trigger a notification.
  store->RemoveLogin(*form);

  // Wait for PasswordStore to send the notification.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();

  store->ShutdownOnUIThread();
}
