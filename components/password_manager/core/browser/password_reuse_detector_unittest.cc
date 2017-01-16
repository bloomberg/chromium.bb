// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using testing::_;

namespace password_manager {

namespace {

std::vector<std::pair<std::string, std::string>> GetTestDomainsPasswords() {
  return {
      {"https://accounts.google.com", "password"},
      {"https://facebook.com", "123456789"},
      {"https://a.appspot.com", "abcdefghi"},
      {"https://twitter.com", "short"},
      {"https://example1.com", "secretword"},
      {"https://example2.com", "secretword"},
  };
}

std::unique_ptr<PasswordForm> GetForm(
    const std::pair<std::string, std::string>& domain_password) {
  std::unique_ptr<PasswordForm> form(new PasswordForm);
  form->signon_realm = domain_password.first;
  form->password_value = ASCIIToUTF16(domain_password.second);
  return form;
}

std::vector<std::unique_ptr<PasswordForm>> GetForms(
    const std::vector<std::pair<std::string, std::string>>& domains_passwords) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  for (const auto& domain_password : domains_passwords)
    result.push_back(GetForm(domain_password));
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

TEST(PasswordReuseDetectorTest, TypingPasswordOnDifferentSite) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(GetTestDomainsPasswords()));
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _, _, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("123passwo"), "https://evil.com",
                            &mockConsumer);
  reuse_detector.CheckReuse(ASCIIToUTF16("123passwor"), "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseFound(ASCIIToUTF16("password"), "google.com", 5, 1));
  reuse_detector.CheckReuse(ASCIIToUTF16("123password"), "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseFound(ASCIIToUTF16("password"), "google.com", 5, 1));
  reuse_detector.CheckReuse(ASCIIToUTF16("password"), "https://evil.com",
                            &mockConsumer);

  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseFound(ASCIIToUTF16("secretword"), "example1.com", 5, 2));
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
  EXPECT_CALL(mockConsumer,
              OnReuseFound(ASCIIToUTF16("abcdefghi"), "a.appspot.com", 5, 1));
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
                  OnReuseFound(ASCIIToUTF16("password"), "google.com", 5, 1));
    }
    reuse_detector.CheckReuse(ASCIIToUTF16("123password"), "https://evil.com",
                              &mockConsumer);
  }
}

TEST(PasswordReuseDetectorTest, CheckLongestPasswordMatchReturn) {
  const std::vector<std::pair<std::string, std::string>> domain_passwords = {
      {"https://example1.com", "234567890"},
      {"https://example2.com", "01234567890"},
      {"https://example3.com", "1234567890"},
  };

  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetForms(domain_passwords));

  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer,
              OnReuseFound(ASCIIToUTF16("01234567890"), "example2.com", 3, 1));
  reuse_detector.CheckReuse(ASCIIToUTF16("abcd01234567890"), "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseFound(ASCIIToUTF16("1234567890"), "example3.com", 3, 1));
  reuse_detector.CheckReuse(ASCIIToUTF16("1234567890"), "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _, _, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("34567890"), "https://evil.com",
                            &mockConsumer);
}

}  // namespace

}  // namespace password_manager
