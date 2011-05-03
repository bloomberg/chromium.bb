// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "chrome/browser/password_manager/password_store_consumer.h"
#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/signaling_task.h"
#include "chrome/test/testing_profile.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer_mock.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;
using testing::_;
using testing::DoAll;
using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;
using testing::WithArg;
using webkit_glue::PasswordForm;

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD2(OnPasswordStoreRequestDone,
               void(CancelableRequestProvider::Handle,
                    const std::vector<webkit_glue::PasswordForm*>&));
};

class MockWebDataServiceConsumer : public WebDataServiceConsumer {
 public:
  MOCK_METHOD2(OnWebDataServiceRequestDone, void(WebDataService::Handle,
                                                 const WDTypedResult*));
};

// This class will add and remove a mock notification observer from
// the DB thread.
class DBThreadObserverHelper :
    public base::RefCountedThreadSafe<DBThreadObserverHelper,
                                      BrowserThread::DeleteOnDBThread> {
 public:
  DBThreadObserverHelper() : done_event_(true, false) {}

  void Init(PasswordStore* password_store) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        NewRunnableMethod(this,
                          &DBThreadObserverHelper::AddObserverTask,
                          make_scoped_refptr(password_store)));
    done_event_.Wait();
  }

  virtual ~DBThreadObserverHelper() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    registrar_.RemoveAll();
  }

  NotificationObserverMock& observer() {
    return observer_;
  }

 protected:
  friend class base::RefCountedThreadSafe<DBThreadObserverHelper>;

  void AddObserverTask(PasswordStore* password_store) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    registrar_.Add(&observer_,
                   NotificationType::LOGINS_CHANGED,
                   Source<PasswordStore>(password_store));
    done_event_.Signal();
  }

  WaitableEvent done_event_;
  NotificationRegistrar registrar_;
  NotificationObserverMock observer_;
};

}  // anonymous namespace

typedef std::vector<PasswordForm*> VectorOfForms;

class PasswordStoreDefaultTest : public testing::Test {
 protected:
  PasswordStoreDefaultTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {
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
    wds_->Shutdown();
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    MessageLoop::current()->Run();
    db_thread_.Stop();
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread db_thread_;  // PasswordStore, WDS schedule work on this thread.

  scoped_ptr<LoginDatabase> login_db_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<WebDataService> wds_;
  ScopedTempDir temp_dir_;
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

TEST_F(PasswordStoreDefaultTest, NonASCIIData) {
  // Prentend that the migration has already taken place.
  profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                            true);

  // Initializing the PasswordStore shouldn't trigger a migration.
  scoped_refptr<PasswordStoreDefault> store(
      new PasswordStoreDefault(login_db_.release(), profile_.get(),
                               wds_.get()));
  store->Init();

  // Some non-ASCII password form data.
  PasswordFormData form_data[] = {
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
  VectorOfForms expected_forms;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(form_data); ++i) {
    PasswordForm* form = CreatePasswordFormFromData(form_data[i]);
    expected_forms.push_back(form);
    store->AddLogin(*form);
  }

  // The PasswordStore schedules tasks to run on the DB thread so we schedule
  // yet another task to notify us that it's safe to carry on with the test.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
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

TEST_F(PasswordStoreDefaultTest, Migration) {
  PasswordFormData autofillable_data[] = {
    { PasswordForm::SCHEME_HTML,
      "http://foo.example.com",
      "http://foo.example.com/origin",
      "http://foo.example.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"password_value",
      true, false, 1 },
    { PasswordForm::SCHEME_HTML,
      "http://bar.example.com",
      "http://bar.example.com/origin",
      "http://bar.example.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"password_value",
      true, false, 2 },
    { PasswordForm::SCHEME_HTML,
      "http://baz.example.com",
      "http://baz.example.com/origin",
      "http://baz.example.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"password_value",
      true, false, 3 },
  };
  PasswordFormData blacklisted_data[] = {
    { PasswordForm::SCHEME_HTML,
      "http://blacklisted.example.com",
      "http://blacklisted.example.com/origin",
      "http://blacklisted.example.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      NULL,
      NULL,
      false, false, 1 },
    { PasswordForm::SCHEME_HTML,
      "http://blacklisted2.example.com",
      "http://blacklisted2.example.com/origin",
      "http://blacklisted2.example.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      NULL,
      NULL,
      false, false, 2 },
  };

  // Build the expected forms vectors and populate the WDS with logins.
  VectorOfForms expected_autofillable;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(autofillable_data); ++i) {
    PasswordForm* form = CreatePasswordFormFromData(autofillable_data[i]);
    expected_autofillable.push_back(form);
    wds_->AddLogin(*form);
  }
  VectorOfForms expected_blacklisted;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(blacklisted_data); ++i) {
    PasswordForm* form = CreatePasswordFormFromData(blacklisted_data[i]);
    expected_blacklisted.push_back(form);
    wds_->AddLogin(*form);
  }

  // The WDS schedules tasks to run on the DB thread so we schedule yet another
  // task to notify us that it's safe to carry on with the test.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  // Initializing the PasswordStore should trigger a migration.
  scoped_refptr<PasswordStore> store(
      new PasswordStoreDefault(login_db_.release(),
          profile_.get(), wds_.get()));
  store->Init();

  // Check that the migration preference has not been initialized;
  ASSERT_TRUE(NULL == profile_->GetPrefs()->FindPreference(
      prefs::kLoginDatabaseMigrated));

  // Again, the WDS schedules tasks to run on the DB thread, so schedule a task
  // to signal us when it is safe to continue.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  // Let the WDS callbacks proceed so the logins can be migrated.
  MessageLoop::current()->RunAllPending();

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnPasswordStoreRequestDone(_, _))
      .WillByDefault(QuitUIMessageLoop());

  // The autofillable forms should have been migrated from the WDS to the login
  // database.
  EXPECT_CALL(consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(expected_autofillable)))
      .WillOnce(DoAll(WithArg<1>(STLDeleteElements0()), QuitUIMessageLoop()));

  store->GetAutofillableLogins(&consumer);
  MessageLoop::current()->Run();

  // The blacklisted forms should have been migrated from the WDS to the login
  // database.
  EXPECT_CALL(consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(expected_blacklisted)))
      .WillOnce(DoAll(WithArg<1>(STLDeleteElements0()), QuitUIMessageLoop()));

  store->GetBlacklistLogins(&consumer);
  MessageLoop::current()->Run();

  // Check that the migration updated the migrated preference.
  ASSERT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kLoginDatabaseMigrated));

  MockWebDataServiceConsumer wds_consumer;

  // No autofillable logins should be left in the WDS.
  EXPECT_CALL(wds_consumer,
      OnWebDataServiceRequestDone(_, EmptyWDResult()));

  wds_->GetAutofillableLogins(&wds_consumer);

  // Wait for the WDS methods to execute on the DB thread.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  // Handle the callback from the WDS.
  MessageLoop::current()->RunAllPending();

  // Likewise, no blacklisted logins should be left in the WDS.
  EXPECT_CALL(wds_consumer,
      OnWebDataServiceRequestDone(_, EmptyWDResult()));

  wds_->GetBlacklistLogins(&wds_consumer);

  // Wait for the WDS methods to execute on the DB thread.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  // Handle the callback from the WDS.
  MessageLoop::current()->RunAllPending();

  STLDeleteElements(&expected_autofillable);
  STLDeleteElements(&expected_blacklisted);

  store->Shutdown();
}

TEST_F(PasswordStoreDefaultTest, MigrationAlreadyDone) {
  PasswordFormData wds_data[] = {
    { PasswordForm::SCHEME_HTML,
      "http://bar.example.com",
      "http://bar.example.com/origin",
      "http://bar.example.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"password_value",
      true, false, 1 },
  };

  // Build the expected forms vector and populate the WDS with logins.
  VectorOfForms unexpected_autofillable;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(wds_data); ++i) {
    PasswordForm* form = CreatePasswordFormFromData(wds_data[i]);
    unexpected_autofillable.push_back(form);
    wds_->AddLogin(*form);
  }

  // The WDS schedules tasks to run on the DB thread so we schedule yet another
  // task to notify us that it's safe to carry on with the test.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  // Prentend that the migration has already taken place.
  profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                            true);

  // Initializing the PasswordStore shouldn't trigger a migration.
  scoped_refptr<PasswordStore> store(
      new PasswordStoreDefault(login_db_.release(), profile_.get(),
                               wds_.get()));
  store->Init();

  MockPasswordStoreConsumer consumer;
  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnPasswordStoreRequestDone(_, _))
      .WillByDefault(QuitUIMessageLoop());

  // No forms should be migrated.
  VectorOfForms empty;
  EXPECT_CALL(consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(empty)))
      .WillOnce(QuitUIMessageLoop());

  store->GetAutofillableLogins(&consumer);
  MessageLoop::current()->Run();

  STLDeleteElements(&unexpected_autofillable);

  store->Shutdown();
}

TEST_F(PasswordStoreDefaultTest, Notifications) {
  // Prentend that the migration has already taken place.
  profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                            true);

  // Initializing the PasswordStore shouldn't trigger a migration.
  scoped_refptr<PasswordStore> store(
      new PasswordStoreDefault(login_db_.release(), profile_.get(),
                               wds_.get()));
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
              Observe(NotificationType(NotificationType::LOGINS_CHANGED),
                      Source<PasswordStore>(store),
                      Property(&Details<const PasswordStoreChangeList>::ptr,
                               Pointee(ElementsAreArray(
                                   expected_add_changes)))));

  // Adding a login should trigger a notification.
  store->AddLogin(*form);

  // The PasswordStore schedules tasks to run on the DB thread so we schedule
  // yet another task to notify us that it's safe to carry on with the test.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  // Change the password.
  form->password_value = WideToUTF16(L"a different password");

  const PasswordStoreChange expected_update_changes[] = {
    PasswordStoreChange(PasswordStoreChange::UPDATE, *form),
  };

  EXPECT_CALL(helper->observer(),
              Observe(NotificationType(NotificationType::LOGINS_CHANGED),
                      Source<PasswordStore>(store),
                      Property(&Details<const PasswordStoreChangeList>::ptr,
                               Pointee(ElementsAreArray(
                                   expected_update_changes)))));

  // Updating the login with the new password should trigger a notification.
  store->UpdateLogin(*form);

  // Wait for PasswordStore to send the notification.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  const PasswordStoreChange expected_delete_changes[] = {
    PasswordStoreChange(PasswordStoreChange::REMOVE, *form),
  };

  EXPECT_CALL(helper->observer(),
              Observe(NotificationType(NotificationType::LOGINS_CHANGED),
                      Source<PasswordStore>(store),
                      Property(&Details<const PasswordStoreChangeList>::ptr,
                               Pointee(ElementsAreArray(
                                   expected_delete_changes)))));

  // Deleting the login should trigger a notification.
  store->RemoveLogin(*form);

  // Wait for PasswordStore to send the notification.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  store->Shutdown();
}
