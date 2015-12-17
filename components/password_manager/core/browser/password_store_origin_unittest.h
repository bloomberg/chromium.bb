// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ORIGIN_UNITTEST_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ORIGIN_UNITTEST_H_

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using autofill::PasswordForm;
using password_manager::PasswordStore;
using testing::_;
using testing::ElementsAre;

namespace password_manager {

PasswordFormData CreateTestPasswordFormDataByOrigin(const char* origin_url) {
  PasswordFormData data = {PasswordForm::SCHEME_HTML, origin_url, origin_url,
                           origin_url, L"submit_element", L"username_element",
                           L"password_element", L"username_value",
                           // Ignore cleared password values on Mac.
                           // crbug.com/466638
                           nullptr, true, false, 1};
  return data;
}

// Collection of origin-related testcases common to all platform-specific
// stores.
// The template type T represents a test delegate that must implement the
// following methods:
//    // Returns a pointer to a fully initialized store for polymorphic usage.
//    PasswordStore* store();
//
//    // Finishes all asnychronous processing on the store.
//    void FinishAsyncProcessing();
template <typename T>
class PasswordStoreOriginTest : public testing::Test {
 protected:
  T delegate_;
};

TYPED_TEST_CASE_P(PasswordStoreOriginTest);

TYPED_TEST_P(PasswordStoreOriginTest,
             RemoveLoginsByOriginAndTimeImpl_FittingOriginAndTime) {
  const char origin_url[] = "http://foo.example.com";
  scoped_ptr<PasswordForm> form = CreatePasswordFormFromDataForTesting(
      CreateTestPasswordFormDataByOrigin(origin_url));
  this->delegate_.store()->AddLogin(*form);
  this->delegate_.FinishAsyncProcessing();

  MockPasswordStoreObserver observer;
  this->delegate_.store()->AddObserver(&observer);

  const url::Origin origin((GURL(origin_url)));
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged(ElementsAre(PasswordStoreChange(
                            PasswordStoreChange::REMOVE, *form))));
  this->delegate_.store()->RemoveLoginsByOriginAndTime(
      origin, base::Time(), base::Time::Max(), run_loop.QuitClosure());
  run_loop.Run();

  this->delegate_.store()->RemoveObserver(&observer);
}

TYPED_TEST_P(PasswordStoreOriginTest,
             RemoveLoginsByOriginAndTimeImpl_NonMatchingOrigin) {
  const char origin_url[] = "http://foo.example.com";
  scoped_ptr<autofill::PasswordForm> form =
      CreatePasswordFormFromDataForTesting(
          CreateTestPasswordFormDataByOrigin(origin_url));
  this->delegate_.store()->AddLogin(*form);
  this->delegate_.FinishAsyncProcessing();

  MockPasswordStoreObserver observer;
  this->delegate_.store()->AddObserver(&observer);

  const url::Origin other_origin(GURL("http://bar.example.com"));
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged(_)).Times(0);
  this->delegate_.store()->RemoveLoginsByOriginAndTime(
      other_origin, base::Time(), base::Time::Max(), run_loop.QuitClosure());
  run_loop.Run();

  this->delegate_.store()->RemoveObserver(&observer);
}

TYPED_TEST_P(PasswordStoreOriginTest,
             RemoveLoginsByOriginAndTimeImpl_NotWithinTimeInterval) {
  const char origin_url[] = "http://foo.example.com";
  scoped_ptr<autofill::PasswordForm> form =
      CreatePasswordFormFromDataForTesting(
          CreateTestPasswordFormDataByOrigin(origin_url));
  this->delegate_.store()->AddLogin(*form);
  this->delegate_.FinishAsyncProcessing();

  MockPasswordStoreObserver observer;
  this->delegate_.store()->AddObserver(&observer);

  const url::Origin origin((GURL(origin_url)));
  base::Time time_after_creation_date =
      form->date_created + base::TimeDelta::FromDays(1);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged(_)).Times(0);
  this->delegate_.store()->RemoveLoginsByOriginAndTime(
      origin, time_after_creation_date, base::Time::Max(),
      run_loop.QuitClosure());
  run_loop.Run();

  this->delegate_.store()->RemoveObserver(&observer);
}

REGISTER_TYPED_TEST_CASE_P(
    PasswordStoreOriginTest,
    RemoveLoginsByOriginAndTimeImpl_FittingOriginAndTime,
    RemoveLoginsByOriginAndTimeImpl_NonMatchingOrigin,
    RemoveLoginsByOriginAndTimeImpl_NotWithinTimeInterval);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ORIGIN_UNITTEST_H_
