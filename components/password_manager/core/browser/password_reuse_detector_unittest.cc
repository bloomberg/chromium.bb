// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using testing::_;

namespace password_manager {

namespace {

using StringVector = std::vector<std::string>;

// Constants to make the tests more readable.
const bool SYNC_REUSE_NO = false;
const bool SYNC_REUSE_YES = true;

std::vector<std::pair<std::string, std::string>> GetTestDomainsPasswords() {
  return {
      {"https://accounts.google.com", "saved_password"},
      {"https://facebook.com", "123456789"},
      {"https://a.appspot.com", "abcdefghi"},
      {"https://twitter.com", "short"},
      {"https://example1.com", "secretword"},
      {"https://example2.com", "secretword"},
  };
}

std::unique_ptr<PasswordForm> GetForm(const std::string& domain,
                                      const std::string& password) {
  std::unique_ptr<PasswordForm> form(new PasswordForm);
  form->signon_realm = domain;
  form->password_value = ASCIIToUTF16(password);
  return form;
}

// Convert a vector of pairs of strings ("domain[,domain...]", "password")
// into a vector of PasswordForms.
std::vector<std::unique_ptr<PasswordForm>> GetForms(
    const std::vector<std::pair<std::string, std::string>>& domains_passwords) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  for (const auto& domains_password : domains_passwords) {
    // Some passwords are used on multiple domains.
    for (const auto& domain :
         base::SplitString(domains_password.first, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL)) {
      result.push_back(GetForm(domain, domains_password.second));
    }
  }
  return result;
}

PasswordStoreChangeList GetChangeList(
    PasswordStoreChange::Type type,
    const std::vector<std::unique_ptr<PasswordForm>>& forms) {
  PasswordStoreChangeList changes;
  for (const auto& form : forms)
    changes.push_back(PasswordStoreChange(type, *form));

  return changes;
}

SyncPasswordData GetSyncPasswordData(const std::string& sync_password) {
  SyncPasswordData sync_password_data;
  sync_password_data.salt = "1234567890123456";
  sync_password_data.length = sync_password.size();
  sync_password_data.hash = HashPasswordManager::CalculateSyncPasswordHash(
      base::ASCIIToUTF16(sync_password), sync_password_data.salt);
  return sync_password_data;
}

TEST(PasswordReuseDetectorTest, TypingPasswordOnDifferentSite) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _, _, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("123saved_passwo"), "https://evil.com",
                            &mockConsumer);
  reuse_detector.CheckReuse(ASCIIToUTF16("123saved_passwor"),
                            "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseFound(strlen("saved_password"), SYNC_REUSE_NO,
                           StringVector({"google.com"}), 5));
  reuse_detector.CheckReuse(ASCIIToUTF16("123saved_password"),
                            "https://evil.com", &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseFound(strlen("saved_password"), SYNC_REUSE_NO,
                           StringVector({"google.com"}), 5));
  reuse_detector.CheckReuse(ASCIIToUTF16("saved_password"), "https://evil.com",
                            &mockConsumer);

  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseFound(strlen("secretword"), SYNC_REUSE_NO,
                           StringVector({"example1.com", "example2.com"}), 5));
  reuse_detector.CheckReuse(ASCIIToUTF16("abcdsecretword"), "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, PSLMatchNoReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _, _, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("123456789"), "https://m.facebook.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, NoPSLMatchReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  // a.appspot.com and b.appspot.com are not PSL matches. So reuse event should
  // be raised.
  EXPECT_CALL(mockConsumer, OnReuseFound(strlen("abcdefghi"), SYNC_REUSE_NO,
                                         StringVector({"a.appspot.com"}), 5));
  reuse_detector.CheckReuse(ASCIIToUTF16("abcdefghi"), "https://b.appspot.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, TooShortPasswordNoReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _, _, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("short"), "evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, PasswordNotInputSuffixNoReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _, _, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("password123"), "https://evil.com",
                            &mockConsumer);
  reuse_detector.CheckReuse(ASCIIToUTF16("123password456"), "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, OnLoginsChanged) {
  for (PasswordStoreChange::Type type :
       {PasswordStoreChange::ADD, PasswordStoreChange::UPDATE,
        PasswordStoreChange::REMOVE}) {
    PasswordReuseDetector reuse_detector;
    PasswordStoreChangeList changes =
        GetChangeList(type, GetForms(GetTestDomainsPasswords()));
    reuse_detector.OnLoginsChanged(changes);
    MockPasswordReuseDetectorConsumer mockConsumer;

    if (type == PasswordStoreChange::REMOVE) {
      EXPECT_CALL(mockConsumer, OnReuseFound(_, _, _, _)).Times(0);
    } else {
      EXPECT_CALL(mockConsumer,
                  OnReuseFound(strlen("saved_password"), SYNC_REUSE_NO,
                               StringVector({"google.com"}), 5));
    }
    reuse_detector.CheckReuse(ASCIIToUTF16("123saved_password"),
                              "https://evil.com", &mockConsumer);
  }
}

TEST(PasswordReuseDetectorTest, MatchMultiplePasswords) {
  // These all have different length passwods so we can check the
  // returned length.
  const std::vector<std::pair<std::string, std::string>> domain_passwords = {
      {"https://a.com, https://all.com", "34567890"},
      {"https://b.com, https://b2.com, https://all.com", "01234567890"},
      {"https://c.com, https://all.com", "1234567890"},
      {"https://d.com", "123456789"},
  };

  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(
      mockConsumer,
      OnReuseFound(
          strlen("01234567890"), SYNC_REUSE_NO,
          StringVector({"a.com", "all.com", "b.com", "b2.com", "c.com"}), 8));
  reuse_detector.CheckReuse(ASCIIToUTF16("abcd01234567890"), "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseFound(strlen("1234567890"), SYNC_REUSE_NO,
                           StringVector({"a.com", "all.com", "c.com"}), 8));
  reuse_detector.CheckReuse(ASCIIToUTF16("1234567890"), "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _, _, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("4567890"), "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, SyncPasswordNoReuse) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::string sync_password = "sync_password";
  reuse_detector.UseSyncPasswordHash(GetSyncPasswordData(sync_password));

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _, _, _)).Times(0);
  // Typing sync password on https://accounts.google.com is OK.
  reuse_detector.CheckReuse(ASCIIToUTF16("123sync_password"),
                            "https://accounts.google.com", &mockConsumer);
  // Only suffixes are verifed.
  reuse_detector.CheckReuse(ASCIIToUTF16("sync_password123"),
                            "https://evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, SyncPasswordReuseFound) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::string sync_password = "sync_password";
  reuse_detector.UseSyncPasswordHash(GetSyncPasswordData(sync_password));

  EXPECT_CALL(mockConsumer, OnReuseFound(strlen("sync_password"),
                                         SYNC_REUSE_YES, StringVector(), 5));
  reuse_detector.CheckReuse(ASCIIToUTF16("sync_password"), "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, MatchSyncAndMultiplePasswords) {
  const std::vector<std::pair<std::string, std::string>> domain_passwords = {
      {"https://a.com", "34567890"}, {"https://b.com", "01234567890"},
  };
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  std::string sync_password = "1234567890";
  reuse_detector.UseSyncPasswordHash(GetSyncPasswordData(sync_password));

  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseFound(strlen("01234567890"), SYNC_REUSE_YES,
                                         StringVector({"a.com", "b.com"}), 2));
  reuse_detector.CheckReuse(ASCIIToUTF16("abcd01234567890"), "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseFound(strlen("1234567890"), SYNC_REUSE_YES,
                                         StringVector(), 2));
  reuse_detector.CheckReuse(ASCIIToUTF16("xyz1234567890"), "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _, _, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("4567890"), "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, SavedPasswordsReuseSyncPasswordAvailable) {
  // Check that reuse of saved passwords is detected also if the sync password
  // hash is saved.
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::string sync_password = "sync_password";
  reuse_detector.UseSyncPasswordHash(GetSyncPasswordData(sync_password));

  EXPECT_CALL(mockConsumer,
              OnReuseFound(strlen("saved_password"), SYNC_REUSE_NO,
                           StringVector({"google.com"}), 5));
  reuse_detector.CheckReuse(ASCIIToUTF16("saved_password"), "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, SavedPasswordAndSyncPasswordReuse) {
  // Verify we can detect that a password matches BOTH the sync password
  // and saved password.
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::string sync_password = "saved_password";  // matches saved password
  reuse_detector.UseSyncPasswordHash(GetSyncPasswordData(sync_password));

  EXPECT_CALL(mockConsumer,
              OnReuseFound(strlen("saved_password"), SYNC_REUSE_YES,
                           StringVector({"google.com"}), 5));
  reuse_detector.CheckReuse(ASCIIToUTF16("saved_password"), "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, ClearSyncPasswordHash) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  std::string sync_password = "sync_password";
  reuse_detector.UseSyncPasswordHash(GetSyncPasswordData(sync_password));
  reuse_detector.ClearSyncPasswordHash();

  // Check that no reuse is found, since hash is cleared.
  reuse_detector.CheckReuse(ASCIIToUTF16("sync_password"), "https://evil.com",
                            &mockConsumer);
}

}  // namespace

}  // namespace password_manager
