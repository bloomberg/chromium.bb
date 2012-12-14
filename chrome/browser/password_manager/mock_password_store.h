// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_H_

#include "chrome/browser/password_manager/password_store.h"
#include "content/public/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;

class MockPasswordStore : public PasswordStore {
 public:
  MockPasswordStore();

  static scoped_refptr<RefcountedProfileKeyedService> Build(Profile* profile);

  MOCK_METHOD1(RemoveLogin, void(const content::PasswordForm&));
  MOCK_METHOD2(GetLogins,
               CancelableTaskTracker::TaskId(const content::PasswordForm&,
                                             PasswordStoreConsumer*));
  MOCK_METHOD1(AddLogin, void(const content::PasswordForm&));
  MOCK_METHOD1(UpdateLogin, void(const content::PasswordForm&));
  MOCK_METHOD0(ReportMetrics, void());
  MOCK_METHOD0(ReportMetricsImpl, void());
  MOCK_METHOD1(AddLoginImpl, void(const content::PasswordForm&));
  MOCK_METHOD1(UpdateLoginImpl, void(const content::PasswordForm&));
  MOCK_METHOD1(RemoveLoginImpl, void(const content::PasswordForm&));
  MOCK_METHOD2(RemoveLoginsCreatedBetweenImpl, void(const base::Time&,
               const base::Time&));
  MOCK_METHOD2(GetLoginsImpl,
               void(const content::PasswordForm& form,
                    const ConsumerCallbackRunner& callback_runner));
  MOCK_METHOD1(GetAutofillableLoginsImpl, void(GetLoginsRequest*));
  MOCK_METHOD1(GetBlacklistLoginsImpl, void(GetLoginsRequest*));
  MOCK_METHOD1(FillAutofillableLogins,
      bool(std::vector<content::PasswordForm*>*));
  MOCK_METHOD1(FillBlacklistLogins,
      bool(std::vector<content::PasswordForm*>*));

  virtual void ShutdownOnUIThread();

 protected:
  virtual ~MockPasswordStore();
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_H_
