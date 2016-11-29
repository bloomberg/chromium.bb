// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "password_reuse_detector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using testing::_;

namespace password_manager {

namespace {

class MockPasswordReuseDetectorConsumer : public PasswordReuseDetectorConsumer {
 public:
  MOCK_METHOD2(OnReuseFound, void(const base::string16&, const std::string&));
};

std::vector<std::unique_ptr<PasswordForm>> GetSavedForms() {
  std::vector<std::unique_ptr<PasswordForm>> result;
  std::vector<std::pair<std::string, std::string>> domains_passwords = {
      {"https://accounts.google.com", "password"},
      {"https://facebook.com", "123456789"},
      {"https://a.appspot.com", "abcdefghi"},
      {"https://twitter.com", "short"},
  };

  for (const auto& domain_password : domains_passwords) {
    std::unique_ptr<PasswordForm> form(new PasswordForm);
    form->signon_realm = domain_password.first;
    form->password_value = ASCIIToUTF16(domain_password.second);
    result.push_back(std::move(form));
  }
  return result;
}

TEST(PasswordReuseDetectorTest, TypingPasswordOnDifferentSite) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetSavedForms());
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("123passwo"), "https://evil.com",
                            &mockConsumer);
  reuse_detector.CheckReuse(ASCIIToUTF16("123passwor"), "https://evil.com",
                            &mockConsumer);
  testing::Mock::VerifyAndClearExpectations(&mockConsumer);

  EXPECT_CALL(mockConsumer,
              OnReuseFound(ASCIIToUTF16("password"), "google.com"));
  reuse_detector.CheckReuse(ASCIIToUTF16("123password"), "https://evil.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, PSLMatchNoReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetSavedForms());
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("123456789"), "https://m.facebook.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, NoPSLMatchReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetSavedForms());
  MockPasswordReuseDetectorConsumer mockConsumer;

  // a.appspot.com and b.appspot.com are not PSL matches. So reuse event should
  // be raised.
  EXPECT_CALL(mockConsumer,
              OnReuseFound(ASCIIToUTF16("abcdefghi"), "a.appspot.com"));
  reuse_detector.CheckReuse(ASCIIToUTF16("abcdefghi"), "https://b.appspot.com",
                            &mockConsumer);
}

TEST(PasswordReuseDetectorTest, TooShortPasswordNoReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetSavedForms());
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("short"), "evil.com", &mockConsumer);
}

TEST(PasswordReuseDetectorTest, PasswordNotInputSuffixNoReuseEvent) {
  PasswordReuseDetector reuse_detector;
  reuse_detector.OnGetPasswordStoreResults(GetSavedForms());
  MockPasswordReuseDetectorConsumer mockConsumer;

  EXPECT_CALL(mockConsumer, OnReuseFound(_, _)).Times(0);
  reuse_detector.CheckReuse(ASCIIToUTF16("password123"), "https://evil.com",
                            &mockConsumer);
  reuse_detector.CheckReuse(ASCIIToUTF16("123password456"), "https://evil.com",
                            &mockConsumer);
}

}  // namespace

}  // namespace password_manager
