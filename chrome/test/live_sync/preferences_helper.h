// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_PREFERENCES_HELPER_H_
#define CHROME_TEST_LIVE_SYNC_PREFERENCES_HELPER_H_
#pragma once

#include "base/file_path.h"
#include "base/values.h"
#include "chrome/test/live_sync/sync_datatype_helper.h"

#include <string>

class PrefService;

class PreferencesHelper : public SyncDatatypeHelper {
 public:
  // Used to access the preferences within a particular sync profile.
  static PrefService* GetPrefs(int index);

  // Used to access the preferences within the verifier sync profile.
  static PrefService* GetVerifierPrefs();

  // Inverts the value of the boolean preference with name |pref_name| in the
  // profile with index |index|. Also inverts its value in |verifier| if
  // DisableVerifier() hasn't been called.
  static void ChangeBooleanPref(int index, const char* pref_name);

  // Changes the value of the integer preference with name |pref_name| in the
  // profile with index |index| to |new_value|. Also changes its value in
  // |verifier| if DisableVerifier() hasn't been called.
  static void ChangeIntegerPref(int index,
                                const char* pref_name,
                                int new_value);

  // Changes the value of the double preference with name |pref_name| in the
  // profile with index |index| to |new_value|. Also changes its value in
  // |verifier| if DisableVerifier() hasn't been called.
  static void ChangeDoublePref(int index,
                               const char* pref_name,
                               double new_value);

  // Changes the value of the string preference with name |pref_name| in the
  // profile with index |index| to |new_value|. Also changes its value in
  // |verifier| if DisableVerifier() hasn't been called.
  static void ChangeStringPref(int index,
                               const char* pref_name,
                               const std::string& new_value);

  // Modifies the value of the string preference with name |pref_name| in the
  // profile with index |index| by appending |append_value| to its current
  // value. Also changes its value in |verifier| if DisableVerifier() hasn't
  // been called.
  static void AppendStringPref(int index,
                               const char* pref_name,
                               const std::string& append_value);

  // Changes the value of the file path preference with name |pref_name| in the
  // profile with index |index| to |new_value|. Also changes its value in
  // |verifier| if DisableVerifier() hasn't been called.
  static void ChangeFilePathPref(int index,
                                 const char* pref_name,
                                 const FilePath& new_value);

  // Changes the value of the list preference with name |pref_name| in the
  // profile with index |index| to |new_value|. Also changes its value in
  // |verifier| if DisableVerifier() hasn't been called.
  static void ChangeListPref(int index,
                             const char* pref_name,
                             const ListValue& new_value);

  // Used to verify that the boolean preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  static bool BooleanPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Used to verify that the integer preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  static bool IntegerPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Used to verify that the double preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  static bool DoublePrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Used to verify that the string preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  static bool StringPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Used to verify that the file path preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  static bool FilePathPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Used to verify that the list preference with name |pref_name| has the
  // same value across all profiles. Also checks |verifier| if DisableVerifier()
  // hasn't been called.
  static bool ListPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

  // Encrypts the Preferences datatype.
  static bool EnableEncryption(int index);

  // Checks if Preferences are encrypted.
  static bool IsEncrypted(int index);

 protected:
  PreferencesHelper();
  virtual ~PreferencesHelper();

 private:
  DISALLOW_COPY_AND_ASSIGN(PreferencesHelper);
};

#endif  // CHROME_TEST_LIVE_SYNC_PREFERENCES_HELPER_H_
