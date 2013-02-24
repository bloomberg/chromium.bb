// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/platform_file.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "chrome/browser/password_manager/password_store_consumer.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
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
using testing::_;
using testing::DoAll;
using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;
using testing::WithArg;
using content::PasswordForm;

typedef std::vector<PasswordForm*> VectorOfForms;

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
                   this, make_scoped_refptr(password_store)));
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

class FailingBackend : public PasswordStoreX::NativeBackend {
 public:
  virtual bool Init() OVERRIDE { return true; }

  virtual bool AddLogin(const PasswordForm& form) OVERRIDE { return false; }
  virtual bool UpdateLogin(const PasswordForm& form) OVERRIDE { return false; }
  virtual bool RemoveLogin(const PasswordForm& form) OVERRIDE { return false; }

  virtual bool RemoveLoginsCreatedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end) OVERRIDE {
    return false;
  }

  virtual bool GetLogins(const PasswordForm& form,
                         PasswordFormList* forms) OVERRIDE {
    return false;
  }

  virtual bool GetLoginsCreatedBetween(const base::Time& get_begin,
                                       const base::Time& get_end,
                                       PasswordFormList* forms) OVERRIDE {
    return false;
  }

  virtual bool GetAutofillableLogins(PasswordFormList* forms) OVERRIDE {
    return false;
  }
  virtual bool GetBlacklistLogins(PasswordFormList* forms) OVERRIDE {
    return false;
  }
};

class MockBackend : public PasswordStoreX::NativeBackend {
 public:
  virtual bool Init() OVERRIDE { return true; }

  virtual bool AddLogin(const PasswordForm& form) OVERRIDE {
    all_forms_.push_back(form);
    return true;
  }

  virtual bool UpdateLogin(const PasswordForm& form) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (CompareForms(all_forms_[i], form, true))
        all_forms_[i] = form;
    return true;
  }

  virtual bool RemoveLogin(const PasswordForm& form) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (CompareForms(all_forms_[i], form, false))
        erase(i--);
    return true;
  }

  virtual bool RemoveLoginsCreatedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (delete_begin <= all_forms_[i].date_created &&
          (delete_end.is_null() || all_forms_[i].date_created < delete_end))
        erase(i--);
    }
    return true;
  }

  virtual bool GetLogins(const PasswordForm& form,
                         PasswordFormList* forms) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (all_forms_[i].signon_realm == form.signon_realm)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

  virtual bool GetLoginsCreatedBetween(const base::Time& get_begin,
                                       const base::Time& get_end,
                                       PasswordFormList* forms) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (get_begin <= all_forms_[i].date_created &&
          (get_end.is_null() || all_forms_[i].date_created < get_end))
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

  virtual bool GetAutofillableLogins(PasswordFormList* forms) OVERRIDE {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (!all_forms_[i].blacklisted_by_user)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

  virtual bool GetBlacklistLogins(PasswordFormList* forms) OVERRIDE {
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

void LoginDatabaseQueryCallback(LoginDatabase* login_db,
                                bool autofillable,
                                MockLoginDatabaseReturn* mock_return) {
  std::vector<PasswordForm*> forms;
  if (autofillable)
    login_db->GetAutofillableLogins(&forms);
  else
    login_db->GetBlacklistLogins(&forms);
  mock_return->OnLoginDatabaseQueryDone(forms);
}

// Generate |count| expected logins, either auto-fillable or blacklisted.
void InitExpectedForms(bool autofillable, size_t count, VectorOfForms* forms) {
  const char* domain = autofillable ? "example" : "blacklisted";
  for (size_t i = 0; i < count; ++i) {
    std::string realm = base::StringPrintf("http://%zu.%s.com", i, domain);
    std::string origin = base::StringPrintf("http://%zu.%s.com/origin",
                                            i, domain);
    std::string action = base::StringPrintf("http://%zu.%s.com/action",
                                            i, domain);
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
      autofillable, false, static_cast<double>(i + 1) };
    forms->push_back(CreatePasswordFormFromData(data));
  }
}

}  // anonymous namespace

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
  }

  virtual void TearDown() {
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
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
  content::TestBrowserThread ui_thread_;
  // PasswordStore, WDS schedule work on this thread.
  content::TestBrowserThread db_thread_;

  scoped_ptr<LoginDatabase> login_db_;
  scoped_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
};

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

ACTION(QuitUIMessageLoop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoop::current()->Quit();
}

TEST_P(PasswordStoreXTest, Notifications) {
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(login_db_.release(),
                         profile_.get(),
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

  // Public in PasswordStore, protected in PasswordStoreX.
  static_cast<PasswordStore*>(store)->ShutdownOnUIThread();
}

TEST_P(PasswordStoreXTest, NativeMigration) {
  VectorOfForms expected_autofillable;
  InitExpectedForms(true, 50, &expected_autofillable);

  VectorOfForms expected_blacklisted;
  InitExpectedForms(false, 50, &expected_blacklisted);

  // Get the initial size of the login DB file, before we populate it.
  // This will be used later to make sure it gets back to this size.
  const base::FilePath login_db_file = temp_dir_.path().Append("login_test");
  base::PlatformFileInfo db_file_start_info;
  ASSERT_TRUE(file_util::GetFileInfo(login_db_file, &db_file_start_info));

  LoginDatabase* login_db = login_db_.get();

  // Populate the login DB with logins that should be migrated.
  for (VectorOfForms::iterator it = expected_autofillable.begin();
       it != expected_autofillable.end(); ++it) {
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
                            base::Bind(
                                base::IgnoreResult(&LoginDatabase::AddLogin),
                                base::Unretained(login_db), **it));
  }
  for (VectorOfForms::iterator it = expected_blacklisted.begin();
       it != expected_blacklisted.end(); ++it) {
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
                            base::Bind(
                                base::IgnoreResult(&LoginDatabase::AddLogin),
                                base::Unretained(login_db), **it));
  }

  // Schedule another task on the DB thread to notify us that it's safe to
  // carry on with the test.
  WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();

  // Get the new size of the login DB file. We expect it to be larger.
  base::PlatformFileInfo db_file_full_info;
  ASSERT_TRUE(file_util::GetFileInfo(login_db_file, &db_file_full_info));
  EXPECT_GT(db_file_full_info.size, db_file_start_info.size);

  // Initializing the PasswordStore shouldn't trigger a native migration (yet).
  scoped_refptr<PasswordStoreX> store(
      new PasswordStoreX(login_db_.release(),
                         profile_.get(),
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

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&LoginDatabaseQueryCallback, login_db, true, &ld_return));

  // Wait for the login DB methods to execute on the DB thread.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
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

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&LoginDatabaseQueryCallback, login_db, false, &ld_return));

  // Wait for the login DB methods to execute on the DB thread.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&WaitableEvent::Signal, base::Unretained(&done)));
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

  // Public in PasswordStore, protected in PasswordStoreX.
  static_cast<PasswordStore*>(store)->ShutdownOnUIThread();
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
