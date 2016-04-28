// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_presenter.h"

#include <stddef.h>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::Eq;
using testing::Property;

namespace {

struct SortEntry {
  const char* const origin;
  const char* const username;
  const char* const password;
  const char* const affiliated_web_realm;
  const char* const federation;
  const int expected_position;
};

}  // namespace

class MockPasswordUIView : public PasswordUIView {
 public:
  explicit MockPasswordUIView(Profile* profile)
      : profile_(profile), password_manager_presenter_(this) {
    password_manager_presenter_.Initialize();
  }
  ~MockPasswordUIView() override {}
  Profile* GetProfile() override;
#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif
  MOCK_METHOD4(ShowPassword, void(
      size_t, const std::string&, const std::string&, const base::string16&));
  MOCK_METHOD1(
      SetPasswordList,
      void(const std::vector<std::unique_ptr<autofill::PasswordForm>>&));
  MOCK_METHOD1(
      SetPasswordExceptionList,
      void(const std::vector<std::unique_ptr<autofill::PasswordForm>>&));
  PasswordManagerPresenter* GetPasswordManagerPresenter() {
    return &password_manager_presenter_;
  }

 private:
  Profile* profile_;
  PasswordManagerPresenter password_manager_presenter_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordUIView);
};

#if !defined(OS_ANDROID)
gfx::NativeWindow MockPasswordUIView::GetNativeWindow() const { return NULL; }
#endif
Profile* MockPasswordUIView::GetProfile() { return profile_; }

class PasswordManagerPresenterTest : public testing::Test {
 protected:
  PasswordManagerPresenterTest() {}

  ~PasswordManagerPresenterTest() override {}
  void SetUp() override {
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        &profile_,
        password_manager::BuildPasswordStore<
            content::BrowserContext, password_manager::MockPasswordStore>);
    mock_controller_.reset(new MockPasswordUIView(&profile_));
  }
  void AddPasswordEntry(const GURL& origin,
                        const std::string& user_name,
                        const std::string& password);
  void AddPasswordException(const GURL& origin);
  void UpdateLists();
  MockPasswordUIView* GetUIController() { return mock_controller_.get(); }
  void SortAndCheckPositions(const SortEntry test_entries[],
                             size_t number_of_entries,
                             PasswordEntryType entry_type);

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<MockPasswordUIView> mock_controller_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPresenterTest);
};

void PasswordManagerPresenterTest::AddPasswordEntry(
    const GURL& origin,
    const std::string& user_name,
    const std::string& password) {
  std::unique_ptr<autofill::PasswordForm> form(new autofill::PasswordForm());
  form->origin = origin;
  form->username_element = base::ASCIIToUTF16("Email");
  form->username_value = base::ASCIIToUTF16(user_name);
  form->password_element = base::ASCIIToUTF16("Passwd");
  form->password_value = base::ASCIIToUTF16(password);
  mock_controller_->GetPasswordManagerPresenter()->password_list_.push_back(
      std::move(form));
}

void PasswordManagerPresenterTest::AddPasswordException(const GURL& origin) {
  std::unique_ptr<autofill::PasswordForm> form(new autofill::PasswordForm());
  form->origin = origin;
  mock_controller_->GetPasswordManagerPresenter()
      ->password_exception_list_.push_back(std::move(form));
}

void PasswordManagerPresenterTest::UpdateLists() {
  mock_controller_->GetPasswordManagerPresenter()->SetPasswordList();
  mock_controller_->GetPasswordManagerPresenter()->SetPasswordExceptionList();
}

void PasswordManagerPresenterTest::SortAndCheckPositions(
    const SortEntry test_entries[],
    size_t number_of_entries,
    PasswordEntryType entry_type) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> list;
  size_t expected_number_of_unique_entries = 0;
  for (size_t i = 0; i < number_of_entries; i++) {
    const SortEntry& entry = test_entries[i];
    std::unique_ptr<autofill::PasswordForm> form(new autofill::PasswordForm());
    form->signon_realm = entry.origin;
    form->origin = GURL(base::ASCIIToUTF16(entry.origin));
    if (entry_type == PasswordEntryType::SAVED) {
      form->username_value = base::ASCIIToUTF16(entry.username);
      form->password_value = base::ASCIIToUTF16(entry.password);
      if (entry.federation != nullptr)
        form->federation_origin = url::Origin(GURL(entry.federation));
    }
    if (entry.affiliated_web_realm)
      form->affiliated_web_realm = entry.affiliated_web_realm;
    list.push_back(std::move(form));
    if (entry.expected_position >= 0)
      expected_number_of_unique_entries++;
  }

  DuplicatesMap duplicates;
  mock_controller_->GetPasswordManagerPresenter()->SortEntriesAndHideDuplicates(
      &list, &duplicates, entry_type);

  ASSERT_EQ(expected_number_of_unique_entries, list.size());
  ASSERT_EQ(number_of_entries - expected_number_of_unique_entries,
            duplicates.size());
  for (size_t i = 0; i < number_of_entries; i++) {
    const SortEntry& entry = test_entries[i];
    if (entry.expected_position >= 0) {
      SCOPED_TRACE(testing::Message("position in sorted list: ")
                   << entry.expected_position);
      EXPECT_EQ(GURL(base::ASCIIToUTF16(entry.origin)),
                list[entry.expected_position]->origin);
      if (entry_type == PasswordEntryType::SAVED) {
        EXPECT_EQ(base::ASCIIToUTF16(entry.username),
                  list[entry.expected_position]->username_value);
        EXPECT_EQ(base::ASCIIToUTF16(entry.password),
                  list[entry.expected_position]->password_value);
        if (entry.federation != nullptr)
          EXPECT_EQ(url::Origin(GURL(entry.federation)),
                    list[entry.expected_position]->federation_origin);
      }
    }
  }
}

namespace {

TEST_F(PasswordManagerPresenterTest, UIControllerIsCalled) {
  EXPECT_CALL(*GetUIController(),
              SetPasswordList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(0u))));
  EXPECT_CALL(*GetUIController(),
              SetPasswordExceptionList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(0u))));
  UpdateLists();
  GURL pass_origin("http://abc1.com");
  AddPasswordEntry(pass_origin, "test@gmail.com", "test");
  EXPECT_CALL(*GetUIController(),
              SetPasswordList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  EXPECT_CALL(*GetUIController(),
              SetPasswordExceptionList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(0u))));
  UpdateLists();
  GURL except_origin("http://abc2.com");
  AddPasswordException(except_origin);
  EXPECT_CALL(*GetUIController(),
              SetPasswordList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  EXPECT_CALL(*GetUIController(),
              SetPasswordExceptionList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  UpdateLists();
  GURL pass_origin2("http://example.com");
  AddPasswordEntry(pass_origin2, "test@gmail.com", "test");
  EXPECT_CALL(*GetUIController(),
              SetPasswordList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(2u))));
  EXPECT_CALL(*GetUIController(),
              SetPasswordExceptionList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  UpdateLists();
}

TEST_F(PasswordManagerPresenterTest, Sorting_DifferentOrigins) {
  const SortEntry test_cases[] = {
      {"http://example-b.com", "user_a", "pwd", nullptr, nullptr, 2},
      {"http://example-a.com", "user_a1", "pwd", nullptr, nullptr, 0},
      {"http://example-a.com", "user_a2", "pwd", nullptr, nullptr, 1},
      {"http://example-c.com", "user_a", "pwd", nullptr, nullptr, 3}};
  SortAndCheckPositions(test_cases, arraysize(test_cases),
                        PasswordEntryType::SAVED);
}

TEST_F(PasswordManagerPresenterTest, Sorting_DifferentUsernames) {
  const SortEntry test_cases[] = {
      {"http://example.com", "user_a", "pwd", nullptr, nullptr, 0},
      {"http://example.com", "user_c", "pwd", nullptr, nullptr, 2},
      {"http://example.com", "user_b", "pwd", nullptr, nullptr, 1}};
  SortAndCheckPositions(test_cases, arraysize(test_cases),
                        PasswordEntryType::SAVED);
}

TEST_F(PasswordManagerPresenterTest, Sorting_DifferentPasswords) {
  const SortEntry test_cases[] = {
      {"http://example.com", "user_a", "1", nullptr, nullptr, 0},
      {"http://example.com", "user_a", "2", nullptr, nullptr, 1},
      {"http://example.com", "user_a", "3", nullptr, nullptr, 2}};
  SortAndCheckPositions(test_cases, arraysize(test_cases),
                        PasswordEntryType::SAVED);
}

TEST_F(PasswordManagerPresenterTest, Sorting_HideDuplicates) {
  const SortEntry test_cases[] = {
      {"http://example.com", "user_a", "pwd", nullptr, nullptr, 0},
      // Different username.
      {"http://example.com", "user_b", "pwd", nullptr, nullptr, 2},
      // Different password.
      {"http://example.com", "user_a", "secret", nullptr, nullptr, 1},
      // Different origin.
      {"http://sub1.example.com", "user_a", "pwd", nullptr, nullptr, 3},
      {"http://example.com", "user_a", "pwd", nullptr, nullptr, -1}  // Hide it.
  };
  SortAndCheckPositions(test_cases, arraysize(test_cases),
                        PasswordEntryType::SAVED);
}

TEST_F(PasswordManagerPresenterTest, Sorting_PasswordExceptions) {
  const SortEntry test_cases[] = {
      {"http://example-b.com", nullptr, nullptr, nullptr, nullptr, 1},
      {"http://example-a.com", nullptr, nullptr, nullptr, nullptr, 0},
      {"http://example-a.com", nullptr, nullptr, nullptr, nullptr,
       -1},  // Hide it.
      {"http://example-c.com", nullptr, nullptr, nullptr, nullptr, 2}};
  SortAndCheckPositions(test_cases, arraysize(test_cases),
                        PasswordEntryType::BLACKLISTED);
}

TEST_F(PasswordManagerPresenterTest, Sorting_AndroidCredentials) {
  const SortEntry test_cases[] = {
      {"https://alpha.com", "user", "secret", nullptr, nullptr, 0},
      {"android://hash@com.alpha", "user", "secret", "https://alpha.com",
       nullptr, 1},
      {"android://hash@com.alpha", "user", "secret", "https://alpha.com",
       nullptr, -1},
      {"android://hash@com.alpha", "user", "secret", nullptr, nullptr, 2},
      {"android://hash@com.betta.android", "user", "secret",
       "https://betta.com", nullptr, 3},
      {"android://hash@com.betta.android", "user", "secret", nullptr, nullptr,
       4}};
  SortAndCheckPositions(test_cases, arraysize(test_cases),
                        PasswordEntryType::SAVED);
}

TEST_F(PasswordManagerPresenterTest, Sorting_Federations) {
  const SortEntry test_cases[] = {
      {"https://example.com", "user", "secret", nullptr, nullptr, 0},
      {"https://example.com", "user", "secret", nullptr, "https://fed1.com", 1},
      {"https://example.com", "user", "secret", nullptr, "https://fed2.com",
       2}};
  SortAndCheckPositions(test_cases, arraysize(test_cases),
                        PasswordEntryType::SAVED);
}

}  // namespace
