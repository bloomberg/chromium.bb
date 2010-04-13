// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/scoped_temp_dir.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "chrome/browser/password_manager/password_store_linux.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::WaitableEvent;
using testing::_;
using testing::DoAll;
using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;
using testing::WithArg;
using webkit_glue::PasswordForm;

namespace {

typedef std::vector<PasswordForm*> VectorOfForms;
typedef std::set<PasswordForm*> SetOfForms;

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD2(OnPasswordStoreRequestDone,
               void(int, const std::vector<webkit_glue::PasswordForm*>&));
};

class MockNotificationObserver : public NotificationObserver {
 public:
  MOCK_METHOD3(Observe, void(NotificationType,
                             const NotificationSource&,
                             const NotificationDetails&));
};

class PasswordStoreLinuxTest : public testing::Test {
 protected:
  PasswordStoreLinuxTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        db_thread_(ChromeThread::DB) {
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
  ChromeThread ui_thread_;
  ChromeThread db_thread_;  // PasswordStore, WDS schedule work on this thread.

  scoped_ptr<LoginDatabase> login_db_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<WebDataService> wds_;
  ScopedTempDir temp_dir_;
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

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

ACTION(QuitUIMessageLoop) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  MessageLoop::current()->Quit();
}

// This matcher is used to check that the |arg| contains exactly the same
// PasswordForms as |forms|, regardless of order.
// TODO(albertb): Convince GMock to pretty-print PasswordForms instead of just
// printing pointers when expectations fail.
MATCHER_P(ContainsAllPasswordForms, forms, "") {
  if (forms.size() != arg.size())
    return false;
  SetOfForms expectations(forms.begin(), forms.end());
  for (unsigned int i = 0; i < arg.size(); ++i) {
    PasswordForm* actual = arg[i];
    bool found_match = false;
    for (SetOfForms::iterator it = expectations.begin();
         it != expectations.end(); ++it) {
      PasswordForm* expected = *it;
      if (expected->scheme == actual->scheme &&
          expected->signon_realm == actual->signon_realm &&
          expected->origin == actual->origin &&
          expected->action == actual->action &&
          expected->submit_element == actual->submit_element &&
          expected->username_element == actual->username_element &&
          expected->password_element == actual->password_element &&
          expected->username_value == actual->username_value &&
          expected->password_value == actual->password_value &&
          expected->blacklisted_by_user == actual->blacklisted_by_user &&
          expected->preferred == actual->preferred &&
          expected->ssl_valid == actual->ssl_valid &&
          expected->date_created == actual->date_created) {
        found_match = true;
        expectations.erase(it);
        break;
      }
    }
    if (!found_match) {
      return false;
    }
  }
  return true;  // Both forms and arg are empty.
}

TEST_F(PasswordStoreLinuxTest, Migration) {
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

  VectorOfForms expected_autofillable;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(autofillable_data); ++i) {
    expected_autofillable.push_back(
        CreatePasswordFormFromData(autofillable_data[i]));
  }

  VectorOfForms expected_blacklisted;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(blacklisted_data); ++i) {
    expected_blacklisted.push_back(
        CreatePasswordFormFromData(blacklisted_data[i]));
  }

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
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, new SignalingTask(&done));
  done.Wait();

  // Initializing the PasswordStore should trigger a migration.
  scoped_refptr<PasswordStoreLinux> store(
      new PasswordStoreLinux(login_db_.release(), profile_.get(), wds_.get()));
  store->Init();

  // Check that the migration preference has not been initialized;
  ASSERT_TRUE(NULL == profile_->GetPrefs()->FindPreference(
      prefs::kLoginDatabaseMigrated));

  // Again, the WDS schedules tasks to run on the DB thread, so schedule a task
  // to signal us when it is safe to continue.
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, new SignalingTask(&done));
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

  STLDeleteElements(&expected_autofillable);
  STLDeleteElements(&expected_blacklisted);
}

TEST_F(PasswordStoreLinuxTest, MigrationAlreadyDone) {
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
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, new SignalingTask(&done));
  done.Wait();

  // Prentend that the migration has already taken place.
  profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                            true);

  // Initializing the PasswordStore shouldn't trigger a migration.
  scoped_refptr<PasswordStoreLinux> store(
      new PasswordStoreLinux(login_db_.release(), profile_.get(), wds_.get()));
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
}
