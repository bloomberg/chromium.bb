// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_PASSWORDS_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_PASSWORDS_SYNC_TEST_H_
#pragma once

#include <set>

#include "chrome/browser/profile.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/ui_test_utils.h"
#include "chrome/test/signaling_task.h"
#include "webkit/glue/password_form.h"

class LivePasswordsSyncTest : public LiveSyncTest {
 public:
  explicit LivePasswordsSyncTest(TestType test_type)
      : LiveSyncTest(test_type) {}

  virtual ~LivePasswordsSyncTest() {}

  // Adds the login held in |form| to the password store |store|. Even though
  // logins are normally added asynchronously, this method will block until the
  // login is added.
  void AddLogin(PasswordStore* store, const webkit_glue::PasswordForm& form) {
    EXPECT_TRUE(store);

    store->AddLogin(form);

    base::WaitableEvent login_added(false, false);
    store->ScheduleTask(new SignalingTask(&login_added));
    login_added.Wait();
  }

  // Searches |store| for all logins matching |form|, the results are added to
  // |matches|. Note that the caller is responsible for deleting the forms added
  // to |matches|.
  void GetLogins(PasswordStore* store,
                 const webkit_glue::PasswordForm& form,
                 std::vector<webkit_glue::PasswordForm>& matches) {
    EXPECT_TRUE(store);

    PasswordStoreConsumerHelper consumer(matches);
    store->GetLogins(form, &consumer);
    ui_test_utils::RunMessageLoop();
  }

  PasswordStore* GetPasswordStore(int index) {
    return GetProfile(index)->GetPasswordStore(Profile::IMPLICIT_ACCESS);
  }

  PasswordStore* GetVerififerPasswordStore() {
    return verifier()->GetPasswordStore(Profile::IMPLICIT_ACCESS);
  }

 private:
  class PasswordStoreConsumerHelper : public PasswordStoreConsumer {
   public:
    explicit PasswordStoreConsumerHelper(
        std::vector<webkit_glue::PasswordForm>& result)
        : PasswordStoreConsumer(), result_(result) {}

    virtual void OnPasswordStoreRequestDone(
        int handle, const std::vector<webkit_glue::PasswordForm*>& result) {
      result_.clear();
      for (std::vector<webkit_glue::PasswordForm*>::const_iterator it =
           result.begin(); it != result.end(); ++it) {
        // Make a copy of the form since it gets deallocated after the caller of
        // this method returns.
        result_.push_back(**it);
      }
      MessageLoopForUI::current()->Quit();
    }

   private:
    std::vector<webkit_glue::PasswordForm>& result_;

    DISALLOW_COPY_AND_ASSIGN(PasswordStoreConsumerHelper);
  };

  DISALLOW_COPY_AND_ASSIGN(LivePasswordsSyncTest);
};

class SingleClientLivePasswordsSyncTest : public LivePasswordsSyncTest {
 public:
  SingleClientLivePasswordsSyncTest()
      : LivePasswordsSyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientLivePasswordsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientLivePasswordsSyncTest);
};

class TwoClientLivePasswordsSyncTest : public LivePasswordsSyncTest {
 public:
  TwoClientLivePasswordsSyncTest() : LivePasswordsSyncTest(TWO_CLIENT) {}
  virtual ~TwoClientLivePasswordsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLivePasswordsSyncTest);
};

class MultipleClientLivePasswordsSyncTest : public LivePasswordsSyncTest {
 public:
  MultipleClientLivePasswordsSyncTest()
      : LivePasswordsSyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientLivePasswordsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientLivePasswordsSyncTest);
};

class ManyClientLivePasswordsSyncTest : public LivePasswordsSyncTest {
 public:
  ManyClientLivePasswordsSyncTest() : LivePasswordsSyncTest(MANY_CLIENT) {}
  virtual ~ManyClientLivePasswordsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ManyClientLivePasswordsSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_PASSWORDS_SYNC_TEST_H_
