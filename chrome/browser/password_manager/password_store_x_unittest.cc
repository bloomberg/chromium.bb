// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/signaling_task.h"
#include "chrome/test/testing_profile.h"
#include "content/common/notification_observer_mock.h"
#include "content/common/notification_service.h"
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

typedef std::vector<PasswordForm*> VectorOfForms;

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD2(OnPasswordStoreRequestDone,
               void(CancelableRequestProvider::Handle,
                    const std::vector<PasswordForm*>&));
};

class MockWebDataServiceConsumer : public WebDataServiceConsumer {
 public:
  MOCK_METHOD2(OnWebDataServiceRequestDone, void(WebDataService::Handle,
                                                 const WDTypedResult*));
};

// This class will add and remove a mock notification observer from
// the DB thread.
class DBThreadObserverHelper
    : public base::RefCountedThreadSafe<DBThreadObserverHelper,
                                        BrowserThread::DeleteOnDBThread> {
 public:
  DBThreadObserverHelper() : done_event_(true, false) {}

  void Init() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        NewRunnableMethod(this, &DBThreadObserverHelper::AddObserverTask));
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

  void AddObserverTask() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    registrar_.Add(&observer_,
                   NotificationType::LOGINS_CHANGED,
                   NotificationService::AllSources());
    done_event_.Signal();
  }

  WaitableEvent done_event_;
  NotificationRegistrar registrar_;
  NotificationObserverMock observer_;
};

class FailingBackend : public PasswordStoreX::NativeBackend {
 public:
  virtual bool Init() { return true; }

  virtual bool AddLogin(const PasswordForm& form) { return false; }
  virtual bool UpdateLogin(const PasswordForm& form) { return false; }
  virtual bool RemoveLogin(const PasswordForm& form) { return false; }

  virtual bool RemoveLoginsCreatedBetween(const base::Time& delete_begin,
                                          const base::Time& delete_end) {
    return false;
  }

  virtual bool GetLogins(const PasswordForm& form, PasswordFormList* forms) {
    return false;
  }

  virtual bool GetLoginsCreatedBetween(const base::Time& get_begin,
                                       const base::Time& get_end,
                                       PasswordFormList* forms) {
    return false;
  }

  virtual bool GetAutofillableLogins(PasswordFormList* forms) { return false; }
  virtual bool GetBlacklistLogins(PasswordFormList* forms) { return false; }
};

class MockBackend : public PasswordStoreX::NativeBackend {
 public:
  virtual bool Init() { return true; }

  virtual bool AddLogin(const PasswordForm& form) {
    all_forms_.push_back(form);
    return true;
  }

  virtual bool UpdateLogin(const PasswordForm& form) {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (CompareForms(all_forms_[i], form, true))
        all_forms_[i] = form;
    return true;
  }

  virtual bool RemoveLogin(const PasswordForm& form) {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (CompareForms(all_forms_[i], form, false))
        erase(i--);
    return true;
  }

  virtual bool RemoveLoginsCreatedBetween(const base::Time& delete_begin,
                                          const base::Time& delete_end) {
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (delete_begin <= all_forms_[i].date_created &&
          (delete_end.is_null() || all_forms_[i].date_created < delete_end))
        erase(i--);
    }
    return true;
  }

  virtual bool GetLogins(const PasswordForm& form, PasswordFormList* forms) {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (all_forms_[i].signon_realm == form.signon_realm)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

  virtual bool GetLoginsCreatedBetween(const base::Time& get_begin,
                                       const base::Time& get_end,
                                       PasswordFormList* forms) {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (get_begin <= all_forms_[i].date_created &&
          (get_end.is_null() || all_forms_[i].date_created < get_end))
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

  virtual bool GetAutofillableLogins(PasswordFormList* forms) {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (!all_forms_[i].blacklisted_by_user)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

  virtual bool GetBlacklistLogins(PasswordFormList* forms) {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (all_forms_[i].blacklisted_by_user)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

 private:
  void erase(size_t index) {
    if (index < all_forms_.size() - 1)
      all_forms_[index] = all_forms_[all_forms_.size() - 1];
    all_forms_.pop_back();
  }

  bool CompareForms(const PasswordForm& a, const PasswordForm& b, bool update) {
    // An update check doesn't care about the submit element.
    if (!update && a.submit_element != b.submit_element)
      return false;
    return a.origin           == b.origin &&
           a.password_element == b.password_element &&
           a.signon_realm     == b.signon_realm &&
           a.username_element == b.username_element &&
           a.username_value   == b.username_value;
  }

  std::vector<PasswordForm> all_forms_;
};

class MockLoginDatabaseReturn {
 public:
  MOCK_METHOD1(OnLoginDatabaseQueryDone,
               void(const std::vector<PasswordForm*>&));
};

class LoginDatabaseQueryTask : public Task {
 public:
  LoginDatabaseQueryTask(LoginDatabase* login_db,
                         bool autofillable,
                         MockLoginDatabaseReturn* mock_return)
      : login_db_(login_db), autofillable_(autofillable),
        mock_return_(mock_return) {
  }

  virtual void Run() {
    std::vector<PasswordForm*> forms;
    if (autofillable_)
      login_db_->GetAutofillableLogins(&forms);
    else
      login_db_->GetBlacklistLogins(&forms);
    mock_return_->OnLoginDatabaseQueryDone(forms);
  }

 private:
  LoginDatabase* login_db_;
  bool autofillable_;
  MockLoginDatabaseReturn* mock_return_;
};

// Generate |count| expected logins, either autofillable or blacklisted.
void InitExpectedForms(bool autofillable, size_t count, VectorOfForms* forms) {
  const char* domain = autofillable ? "example" : "blacklisted";
  for (size_t i = 0; i < count; ++i) {
    std::string realm = StringPrintf("http://%zu.%s.com", i, domain);
    std::string origin = StringPrintf("http://%zu.%s.com/origin", i, domain);
    std::string action = StringPrintf("http://%zu.%s.com/action", i, domain);
    PasswordFormData data = {
      PasswordForm::SCHEME_HTML,
      realm.c_str(),
      origin.c_str(),
      action.c_str(),
      L"submit_element",
      L"username_element",
      L"password_element",
      autofillable ? L"username_value" : NULL,
      autofillable ? L"password_value" : NULL,
      autofillable, false, i + 1 };
    forms->push_back(CreatePasswordFormFromData(data));
  }
}

}  // anonymous namespace

// LoginDatabase isn't reference counted, but in these unit tests that won't be
// a problem as it always outlives the threads we post tasks to.
template<>
struct RunnableMethodTraits<LoginDatabase> {
  void RetainCallee(LoginDatabase*) {}
  void ReleaseCallee(LoginDatabase*) {}
};

enum BackendType {
  NO_BACKEND,
  FAILING_BACKEND,
  WORKING_BACKEND
};

class PasswordStoreXTest : public testing::TestWithParam<BackendType> {
 protected:
  PasswordStoreXTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(db_thread_.Start());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    profile_.reset(new TestingProfile());

    login_db_.reset(new LoginDatabase());
    ASSERT_TRUE(login_db_->Init(temp_dir_.path().Append("login_test")));

    wds_ = new WebDataService();
    ASSERT_TRUE(wds_->Init(temp_dir_.path()));
  }

  virtual void TearDown() {
    wds_->Shutdown();
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    MessageLoop::current()->Run();
    db_thread_.Stop();
  }

  PasswordStoreX::NativeBackend* GetBackend() {
    switch (GetParam()) {
      case FAILING_BACKEND:
        return new FailingBackend();
      case WORKING_BACKEND:
        return new MockBackend();
      default:
        return NULL;
    }
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

TEST_P(PasswordStoreXTest, WDSMigration) {
  VectorOfForms expected_autofillable;
  InitExpectedForms(true, 5, &expected_autofillable);

  VectorOfForms expected_blacklisted;
  InitExpectedForms(false, 5, &expected_blacklisted);

  // Populate the WDS with logins that should be migrated.
  for (VectorOfForms::iterator it = expected_autofillable.begin();
       it != expected_autofillable.end(); ++it) {
    wds_->AddLogin(**it);
  }
  for (VectorOfForms::iterator it = expected_blacklisted.begin();
       it != expected_blacklisted.end(); ++it) {
    wds_->AddLogin(**it);
  }

  // The WDS schedules tasks to run on the DB thread so we schedule yet another
  // task to notify us that it's safe to carry on with the test.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  // Initializing the PasswordStore should trigger a migration.
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(login_db_.release(),
                           profile_.get(),
                           wds_.get(),
                           GetBackend()));
  store->Init();

  // Check that the migration preference has not been initialized.
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

TEST_P(PasswordStoreXTest, WDSMigrationAlreadyDone) {
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

  VectorOfForms unexpected_autofillable;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(wds_data); ++i) {
    unexpected_autofillable.push_back(
        CreatePasswordFormFromData(wds_data[i]));
  }

  // Populate the WDS with logins that should be migrated.
  for (VectorOfForms::iterator it = unexpected_autofillable.begin();
       it != unexpected_autofillable.end(); ++it) {
    wds_->AddLogin(**it);
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
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(login_db_.release(),
                           profile_.get(),
                           wds_.get(),
                           GetBackend()));
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

TEST_P(PasswordStoreXTest, Notifications) {
  // Pretend that the migration has already taken place.
  profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                            true);

  // Initializing the PasswordStore shouldn't trigger a migration.
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(login_db_.release(),
                           profile_.get(),
                           wds_.get(),
                           GetBackend()));
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
  helper->Init();

  const PasswordStoreChange expected_add_changes[] = {
    PasswordStoreChange(PasswordStoreChange::ADD, *form),
  };

  EXPECT_CALL(helper->observer(),
              Observe(NotificationType(NotificationType::LOGINS_CHANGED),
                      NotificationService::AllSources(),
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
                      NotificationService::AllSources(),
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
                      NotificationService::AllSources(),
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

TEST_P(PasswordStoreXTest, NativeMigration) {
  VectorOfForms expected_autofillable;
  InitExpectedForms(true, 50, &expected_autofillable);

  VectorOfForms expected_blacklisted;
  InitExpectedForms(false, 50, &expected_blacklisted);

  // Get the initial size of the login DB file, before we populate it.
  // This will be used later to make sure it gets back to this size.
  const FilePath login_db_file = temp_dir_.path().Append("login_test");
  base::PlatformFileInfo db_file_start_info;
  ASSERT_TRUE(file_util::GetFileInfo(login_db_file, &db_file_start_info));

  LoginDatabase* login_db = login_db_.get();

  // Populate the login DB with logins that should be migrated.
  for (VectorOfForms::iterator it = expected_autofillable.begin();
       it != expected_autofillable.end(); ++it) {
    BrowserThread::PostTask(BrowserThread::DB,
                           FROM_HERE,
                           NewRunnableMethod(login_db,
                                             &LoginDatabase::AddLogin,
                                             **it));
  }
  for (VectorOfForms::iterator it = expected_blacklisted.begin();
       it != expected_blacklisted.end(); ++it) {
    BrowserThread::PostTask(BrowserThread::DB,
                           FROM_HERE,
                           NewRunnableMethod(login_db,
                                             &LoginDatabase::AddLogin,
                                             **it));
  }

  // Schedule another task on the DB thread to notify us that it's safe to
  // carry on with the test.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  // Get the new size of the login DB file. We expect it to be larger.
  base::PlatformFileInfo db_file_full_info;
  ASSERT_TRUE(file_util::GetFileInfo(login_db_file, &db_file_full_info));
  EXPECT_GT(db_file_full_info.size, db_file_start_info.size);

  // Pretend that the WDS migration has already taken place.
  profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                            true);

  // Initializing the PasswordStore shouldn't trigger a native migration (yet).
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(login_db_.release(),
                           profile_.get(),
                           wds_.get(),
                           GetBackend()));
  store->Init();

  MockPasswordStoreConsumer consumer;

  // Make sure we quit the MessageLoop even if the test fails.
  ON_CALL(consumer, OnPasswordStoreRequestDone(_, _))
      .WillByDefault(QuitUIMessageLoop());

  // The autofillable forms should have been migrated to the native backend.
  EXPECT_CALL(consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(expected_autofillable)))
      .WillOnce(DoAll(WithArg<1>(STLDeleteElements0()), QuitUIMessageLoop()));

  store->GetAutofillableLogins(&consumer);
  MessageLoop::current()->Run();

  // The blacklisted forms should have been migrated to the native backend.
  EXPECT_CALL(consumer,
      OnPasswordStoreRequestDone(_,
          ContainsAllPasswordForms(expected_blacklisted)))
      .WillOnce(DoAll(WithArg<1>(STLDeleteElements0()), QuitUIMessageLoop()));

  store->GetBlacklistLogins(&consumer);
  MessageLoop::current()->Run();

  VectorOfForms empty;
  MockLoginDatabaseReturn ld_return;

  if (GetParam() == WORKING_BACKEND) {
    // No autofillable logins should be left in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(ContainsAllPasswordForms(empty)));
  } else {
    // The autofillable logins should still be in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(
                    ContainsAllPasswordForms(expected_autofillable)))
        .WillOnce(WithArg<0>(STLDeleteElements0()));
  }

  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new LoginDatabaseQueryTask(login_db, true, &ld_return));

  // Wait for the login DB methods to execute on the DB thread.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  if (GetParam() == WORKING_BACKEND) {
    // Likewise, no blacklisted logins should be left in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(ContainsAllPasswordForms(empty)));
  } else {
    // The blacklisted logins should still be in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(
                    ContainsAllPasswordForms(expected_blacklisted)))
        .WillOnce(WithArg<0>(STLDeleteElements0()));
  }

  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new LoginDatabaseQueryTask(login_db, false, &ld_return));

  // Wait for the login DB methods to execute on the DB thread.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      new SignalingTask(&done));
  done.Wait();

  if (GetParam() == WORKING_BACKEND) {
    // If the migration succeeded, then not only should there be no logins left
    // in the login DB, but also the file should have been deleted and then
    // recreated. We approximate checking for this by checking that the file
    // size is equal to the size before we populated it, even though it was
    // larger after populating it.
    base::PlatformFileInfo db_file_end_info;
    ASSERT_TRUE(file_util::GetFileInfo(login_db_file, &db_file_end_info));
    EXPECT_EQ(db_file_start_info.size, db_file_end_info.size);
  }

  STLDeleteElements(&expected_autofillable);
  STLDeleteElements(&expected_blacklisted);

  store->Shutdown();
}

INSTANTIATE_TEST_CASE_P(NoBackend,
                        PasswordStoreXTest,
                        testing::Values(NO_BACKEND));
INSTANTIATE_TEST_CASE_P(FailingBackend,
                        PasswordStoreXTest,
                        testing::Values(FAILING_BACKEND));
INSTANTIATE_TEST_CASE_P(WorkingBackend,
                        PasswordStoreXTest,
                        testing::Values(WORKING_BACKEND));
