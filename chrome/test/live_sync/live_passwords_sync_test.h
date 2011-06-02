// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_PASSWORDS_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_PASSWORDS_SYNC_TEST_H_
#pragma once

#include <vector>

#include "base/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "webkit/glue/password_form.h"

class PasswordStore;

class LivePasswordsSyncTest : public LiveSyncTest {
 public:
  explicit LivePasswordsSyncTest(TestType test_type);
  virtual ~LivePasswordsSyncTest() {}

  // Overrides LiveSyncTest::CleanUpOnMainThread. Cleans up password forms that
  // were added by a test.
  virtual void CleanUpOnMainThread();

  // Overrides LiveSyncTest::SetupClients. Initializes sync clients and profiles
  // and cleans up old password forms that might have been left behind on the
  // host machine by a previous test run that crashed before it could clean up
  // after itself.
  virtual bool SetupClients() WARN_UNUSED_RESULT;

  // Adds the login held in |form| to the password store |store|. Even though
  // logins are normally added asynchronously, this method will block until the
  // login is added.
  void AddLogin(PasswordStore* store,
                const webkit_glue::PasswordForm& form);

  // Searches |store| for all logins matching a fake signon realm used only by
  // LivePasswordsSyncTest and adds the results to |matches|. Note that the
  // caller is responsible for deleting the forms added to |matches|.
  void GetLogins(PasswordStore* store,
                 std::vector<webkit_glue::PasswordForm>& matches);


  // Sets the cryptographer's passphrase for the profile at index |index| to
  // |passphrase|. |is_creation| is true if a new passphrase is being set up
  // and false otherwise.
  void SetPassphrase(int index,
                     const std::string& passphrase,
                     bool is_creation);

  // Gets the password store of the profile with index |index|.
  PasswordStore* GetPasswordStore(int index);

  // Gets the password store of the verifier profile.
  PasswordStore* GetVerifierPasswordStore();

  // Returns true iff the profile with index |index| contains the same password
  // forms as the verifier profile.
  bool ProfileContainsSamePasswordFormsAsVerifier(int index);

  // Returns true iff the profile with index |index_a| contains the same
  // password forms as the profile with index |index_b|.
  bool ProfilesContainSamePasswordForms(int index_a, int index_b);

  // Returns true iff all profiles contain the same password forms as the
  // verifier profile.
  bool AllProfilesContainSamePasswordFormsAsVerifier();

  // Returns true iff all profiles contain the same password forms.
  bool AllProfilesContainSamePasswordForms();

  // Returns the number of forms in the password store of the profile with index
  // |index|.
  int GetPasswordCount(int index);

  // Returns the number of forms in the password store of the verifier profile.
  int GetVerifierPasswordCount();

  // Creates a test password form with a well known fake signon realm used only
  // by LivePasswordsSyncTest based on |index|.
  webkit_glue::PasswordForm CreateTestPasswordForm(int index);

 private:
  // Cleans up all password forms ever added by LivePasswordsSyncTest. This is
  // done by matching existing password forms found on a machine against a known
  // fake sign-on realm used only by LivePasswordsSyncTest.
  void CleanupTestPasswordForms();

  DISALLOW_COPY_AND_ASSIGN(LivePasswordsSyncTest);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(LivePasswordsSyncTest);

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
