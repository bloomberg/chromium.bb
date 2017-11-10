// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_presenter.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/browser/ui/passwords/password_ui_view_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::Eq;
using testing::Property;

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

}  // namespace
