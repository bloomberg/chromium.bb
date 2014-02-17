// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/mock_password_store_service.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::Eq;
using testing::Property;

class MockPasswordUIView : public PasswordUIView {
 public:
  explicit MockPasswordUIView(Profile* profile)
      : profile_(profile), password_manager_presenter_(this) {
    password_manager_presenter_.Initialize();
  }
  virtual ~MockPasswordUIView() {}
  virtual Profile* GetProfile() OVERRIDE;
#if !defined(OS_ANDROID)
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
#endif
  MOCK_METHOD2(ShowPassword, void(size_t, const base::string16&));
  MOCK_METHOD2(SetPasswordList,
               void(const ScopedVector<autofill::PasswordForm>&, bool));
  MOCK_METHOD1(SetPasswordExceptionList,
               void(const ScopedVector<autofill::PasswordForm>&));
  PasswordManagerPresenter* GetPasswordManagerPresenter() {
    return &password_manager_presenter_;
  }

 private:
  Profile* profile_;
  PasswordManagerPresenter password_manager_presenter_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordUIView);
};

#if !defined(OS_ANDROID)
gfx::NativeWindow MockPasswordUIView::GetNativeWindow() { return NULL; }
#endif
Profile* MockPasswordUIView::GetProfile() { return profile_; }

class PasswordManagerPresenterTest : public testing::Test {
 protected:
  PasswordManagerPresenterTest() {}

  virtual ~PasswordManagerPresenterTest() {}
  virtual void SetUp() OVERRIDE {
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        &profile_, MockPasswordStoreService::Build);
    mock_controller_.reset(new MockPasswordUIView(&profile_));
  }
  void AddPasswordEntry(const GURL& origin,
                        const std::string& user_name,
                        const std::string& password);
  void AddPasswordException(const GURL& origin);
  void UpdateLists();
  MockPasswordUIView* GetUIController() { return mock_controller_.get(); }

 private:
  TestingProfile profile_;
  scoped_ptr<MockPasswordUIView> mock_controller_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPresenterTest);
};

void PasswordManagerPresenterTest::AddPasswordEntry(
    const GURL& origin,
    const std::string& user_name,
    const std::string& password) {
  autofill::PasswordForm* form = new autofill::PasswordForm();
  form->origin = origin;
  form->username_element = base::ASCIIToUTF16("Email");
  form->username_value = base::ASCIIToUTF16(user_name);
  form->password_element = base::ASCIIToUTF16("Passwd");
  form->password_value = base::ASCIIToUTF16(password);
  mock_controller_->GetPasswordManagerPresenter()->password_list_
      .push_back(form);
}

void PasswordManagerPresenterTest::AddPasswordException(const GURL& origin) {
  autofill::PasswordForm* form = new autofill::PasswordForm();
  form->origin = origin;
  mock_controller_->GetPasswordManagerPresenter()->password_exception_list_
      .push_back(form);
}

void PasswordManagerPresenterTest::UpdateLists() {
  mock_controller_->GetPasswordManagerPresenter()->SetPasswordList();
  mock_controller_->GetPasswordManagerPresenter()->SetPasswordExceptionList();
}

namespace {

TEST_F(PasswordManagerPresenterTest, UIControllerIsCalled) {
  EXPECT_CALL(
      *GetUIController(),
      SetPasswordList(
          Property(&ScopedVector<autofill::PasswordForm>::size, Eq(0u)),
          testing::_));
  EXPECT_CALL(*GetUIController(),
              SetPasswordExceptionList(Property(
                  &ScopedVector<autofill::PasswordForm>::size, Eq(0u))));
  UpdateLists();
  GURL pass_origin("http://abc1.com");
  AddPasswordEntry(pass_origin, "test@gmail.com", "test");
  EXPECT_CALL(
      *GetUIController(),
      SetPasswordList(
          Property(&ScopedVector<autofill::PasswordForm>::size, Eq(1u)),
          testing::_));
  EXPECT_CALL(*GetUIController(),
              SetPasswordExceptionList(Property(
                  &ScopedVector<autofill::PasswordForm>::size, Eq(0u))));
  UpdateLists();
  GURL except_origin("http://abc2.com");
  AddPasswordException(except_origin);
  EXPECT_CALL(
      *GetUIController(),
      SetPasswordList(
          Property(&ScopedVector<autofill::PasswordForm>::size, Eq(1u)),
          testing::_));
  EXPECT_CALL(*GetUIController(),
              SetPasswordExceptionList(Property(
                  &ScopedVector<autofill::PasswordForm>::size, Eq(1u))));
  UpdateLists();
  GURL pass_origin2("http://example.com");
  AddPasswordEntry(pass_origin2, "test@gmail.com", "test");
  EXPECT_CALL(
      *GetUIController(),
      SetPasswordList(
          Property(&ScopedVector<autofill::PasswordForm>::size, Eq(2u)),
          testing::_));
  EXPECT_CALL(*GetUIController(),
              SetPasswordExceptionList(Property(
                  &ScopedVector<autofill::PasswordForm>::size, Eq(1u))));
  UpdateLists();
}

}  // namespace
