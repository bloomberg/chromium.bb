// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
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
using testing::Field;
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
    mock_store_ = static_cast<password_manager::MockPasswordStore*>(
        PasswordStoreFactory::GetForProfile(&profile_,
                                            Profile::EXPLICIT_ACCESS).get());
  }
  void AddPasswordEntry(const GURL& origin,
                        const std::string& user_name,
                        const std::string& password);
  void AddPasswordException(const GURL& origin);
  void UpdateLists();
  MockPasswordUIView* GetUIController() { return mock_controller_.get(); }
  PasswordManagerPresenter* GetPasswordManagerPresenter() {
    return mock_controller_->GetPasswordManagerPresenter();
  }
  password_manager::MockPasswordStore* GetPasswordStore() {
    return mock_store_.get();
  }

 private:
  TestingProfile profile_;
  scoped_ptr<MockPasswordUIView> mock_controller_;
  scoped_refptr<password_manager::MockPasswordStore> mock_store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPresenterTest);
};

void PasswordManagerPresenterTest::AddPasswordEntry(
    const GURL& origin,
    const std::string& user_name,
    const std::string& password) {
  autofill::PasswordForm* form = new autofill::PasswordForm();
  form->origin = origin;
  form->username_element = ASCIIToUTF16("Email");
  form->username_value = ASCIIToUTF16(user_name);
  form->password_element = ASCIIToUTF16("Passwd");
  form->password_value = ASCIIToUTF16(password);
  GetPasswordManagerPresenter()->password_list_.push_back(form);
}

void PasswordManagerPresenterTest::AddPasswordException(const GURL& origin) {
  autofill::PasswordForm* form = new autofill::PasswordForm();
  form->origin = origin;
  GetPasswordManagerPresenter()->password_exception_list_.push_back(form);
}

void PasswordManagerPresenterTest::UpdateLists() {
  GetPasswordManagerPresenter()->SetPasswordList();
  GetPasswordManagerPresenter()->SetPasswordExceptionList();
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

// AddPassword and UpdatePassword are never called on Android.
#if !defined(OS_ANDROID)
TEST_F(PasswordManagerPresenterTest, CallAddPassword) {
  GURL basic_origin("http://host.com");
  base::string16 username = ASCIIToUTF16("username");
  base::string16 password = ASCIIToUTF16("password");
  EXPECT_CALL(
      *GetPasswordStore(),
      AddLogin(testing::AllOf(
          Field(&autofill::PasswordForm::signon_realm, Eq(basic_origin.spec())),
          Field(&autofill::PasswordForm::origin, Eq(basic_origin)),
          Field(&autofill::PasswordForm::username_value, Eq(username)),
          Field(&autofill::PasswordForm::password_value, Eq(password)),
          Field(&autofill::PasswordForm::ssl_valid, Eq(false)))));
  GetPasswordManagerPresenter()->AddPassword(basic_origin, username, password);

  GURL complex_origin("https://foo:bar@host.com:1234/path?query=v#ref");
  EXPECT_CALL(
      *GetPasswordStore(),
      AddLogin(testing::AllOf(
          Field(&autofill::PasswordForm::signon_realm,
                Eq("https://host.com:1234/")),
          Field(&autofill::PasswordForm::origin,
                Eq(GURL("https://host.com:1234/path"))),
          Field(&autofill::PasswordForm::username_value, Eq(username)),
          Field(&autofill::PasswordForm::password_value, Eq(password)),
          Field(&autofill::PasswordForm::ssl_valid, Eq(true)))));
  GetPasswordManagerPresenter()->AddPassword(complex_origin,
                                             username,
                                             password);
}

TEST_F(PasswordManagerPresenterTest, CallUpdatePassword) {
  GURL origin1("http://host.com");
  static const char kUsername1[] = "username";
  AddPasswordEntry(origin1, kUsername1, "password");
  GURL origin2("https://example.com");
  static const char kUsername2[] = "testname";
  AddPasswordEntry(origin2, kUsername2, "abcd");

  base::string16 new_password = ASCIIToUTF16("testpassword");
  EXPECT_CALL(
      *GetPasswordStore(),
      UpdateLogin(testing::AllOf(
          Field(&autofill::PasswordForm::origin, Eq(origin1)),
          Field(&autofill::PasswordForm::username_value,
                Eq(ASCIIToUTF16(kUsername1))),
          Field(&autofill::PasswordForm::password_value,
                Eq(new_password)))));
  GetPasswordManagerPresenter()->UpdatePassword(0, new_password);

  base::string16 new_password_again = ASCIIToUTF16("testpassword_again");
  EXPECT_CALL(
      *GetPasswordStore(),
      UpdateLogin(testing::AllOf(
          Field(&autofill::PasswordForm::origin, Eq(origin1)),
          Field(&autofill::PasswordForm::username_value,
                Eq(ASCIIToUTF16(kUsername1))),
          Field(&autofill::PasswordForm::password_value,
                Eq(new_password_again)))));
  GetPasswordManagerPresenter()->UpdatePassword(0, new_password_again);

  base::string16 another_password = ASCIIToUTF16("mypassword");
  EXPECT_CALL(
      *GetPasswordStore(),
      UpdateLogin(testing::AllOf(
          Field(&autofill::PasswordForm::origin, Eq(origin2)),
          Field(&autofill::PasswordForm::username_value,
                Eq(ASCIIToUTF16(kUsername2))),
          Field(&autofill::PasswordForm::password_value,
                Eq(another_password)))));
  GetPasswordManagerPresenter()->UpdatePassword(1, another_password);
}
#endif  // !defined(OS_ANDROID)

TEST(PasswordManagerPresenterTestSimple, CallCheckOriginValidityForAdding) {
  static const char* const kValidOrigins[] = {
    "http://host.com",
    "http://host.com/path",
    "https://host.com",
    "https://foo:bar@host.com/path?query=v#ref",
    "https://foo:bar@host.com:1234/path?query=v#ref"
  };
  for (size_t i = 0; i < arraysize(kValidOrigins); ++i) {
    SCOPED_TRACE(kValidOrigins[i]);
    EXPECT_TRUE(PasswordManagerPresenter::CheckOriginValidityForAdding(
        GURL(kValidOrigins[i])));
  }

  static const char* const kInvalidOrigins[] = {
    "noscheme",
    "invalidscheme:host.com",
    "ftp://ftp.host.com",
    "about:test"
  };
  for (size_t i = 0; i < arraysize(kInvalidOrigins); ++i) {
    SCOPED_TRACE(kInvalidOrigins[i]);
    EXPECT_FALSE(PasswordManagerPresenter::CheckOriginValidityForAdding(
        GURL(kInvalidOrigins[i])));
  }
}

}  // namespace
