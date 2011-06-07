// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_PREFERENCES_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_PREFERENCES_SYNC_TEST_H_
#pragma once

#include <string>
#include "chrome/test/live_sync/live_sync_test.h"

class PrefService;

class LivePreferencesSyncTest : public LiveSyncTest {
 public:
  explicit LivePreferencesSyncTest(TestType test_type);
  virtual ~LivePreferencesSyncTest();

  // Used to access the preferences within a particular sync profile.
  PrefService* GetPrefs(int index);

  // Used to access the preferences within the verifier sync profile.
  PrefService* GetVerifierPrefs();

  // Inverts the value of the boolean preference with name |pref_name| in the
  // profile with index |index|. Also inverts its value in |verifier| if
  // DisableVerifier() hasn't been called.
  void ChangeBooleanPref(int index, const char* pref_name);

  // Changes the value of the integer preference with name |pref_name| in the
  // profile with index |index| to |new_value|. Also changes its value in
  // |verifier| if DisableVerifier() hasn't been called.
  void ChangeIntegerPref(int index, const char* pref_name, int new_value);

  // Changes the value of the double preference with name |pref_name| in the
  // profile with index |index| to |new_value|. Also changes its value in
  // |verifier| if DisableVerifier() hasn't been called.
  void ChangeDoublePref(int index, const char* pref_name, double new_value);

  // Changes the value of the string preference with name |pref_name| in the
  // profile with index |index| to |new_value|. Also changes its value in
  // |verifier| if DisableVerifier() hasn't been called.
  void ChangeStringPref(int index,
                        const char* pref_name,
                        const std::string& new_value);

  // Modifies the value of the string preference with name |pref_name| in the
  // profile with index |index| by appending |append_value| to its current
  // value. Also changes its value in |verifier| if DisableVerifier() hasn't
  // been called.
  void AppendStringPref(int index,
                        const char* pref_name,
                        const std::string& append_value);

  // Changes the value of the file path preference with name |pref_name| in the
  // profile with index |index| to |new_value|. Also changes its value in
  // |verifier| if DisableVerifier() hasn't been called.
  void ChangeFilePathPref(int index,
                          const char* pref_name,
                          const FilePath& new_value);

  // Changes the value of the list preference with name |pref_name| in the
  // profile with index |index| to |new_value|. Also changes its value in
  // |verifier| if DisableVerifier() hasn't been called.
  void ChangeListPref(int index,
                      const char* pref_name,
                      const ListValue& new_value);

  // Used to verify that the boolean preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  bool BooleanPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Used to verify that the integer preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  bool IntegerPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Used to verify that the double preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  bool DoublePrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Used to verify that the string preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  bool StringPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Used to verify that the file path preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  bool FilePathPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Used to verify that the list preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  bool ListPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // After calling this method, changes made to a profile's PrefService will no
  // longer be reflected in the verifier profile.
  void DisableVerifier();

 private:
  // Indicates whether preference operations should also update the verifier
  // profile's PrefService or not.
  bool use_verifier_prefs_;

  DISALLOW_COPY_AND_ASSIGN(LivePreferencesSyncTest);
};

class SingleClientLivePreferencesSyncTest : public LivePreferencesSyncTest {
 public:
  SingleClientLivePreferencesSyncTest()
      : LivePreferencesSyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientLivePreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientLivePreferencesSyncTest);
};

class TwoClientLivePreferencesSyncTest : public LivePreferencesSyncTest {
 public:
  TwoClientLivePreferencesSyncTest() : LivePreferencesSyncTest(TWO_CLIENT) {}
  virtual ~TwoClientLivePreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLivePreferencesSyncTest);
};

class MultipleClientLivePreferencesSyncTest : public LivePreferencesSyncTest {
 public:
  MultipleClientLivePreferencesSyncTest()
      : LivePreferencesSyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientLivePreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientLivePreferencesSyncTest);
};

class ManyClientLivePreferencesSyncTest : public LivePreferencesSyncTest {
 public:
  ManyClientLivePreferencesSyncTest() : LivePreferencesSyncTest(MANY_CLIENT) {}
  virtual ~ManyClientLivePreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ManyClientLivePreferencesSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_PREFERENCES_SYNC_TEST_H_
