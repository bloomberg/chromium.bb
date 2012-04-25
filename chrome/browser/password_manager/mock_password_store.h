// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_H_

#include "chrome/browser/password_manager/password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/forms/password_form.h"

class Profile;

class MockPasswordStore : public PasswordStore {
 public:
  MockPasswordStore();

  static scoped_refptr<RefcountedProfileKeyedService> Build(Profile* profile);

  MOCK_METHOD1(RemoveLogin, void(const webkit::forms::PasswordForm&));
  MOCK_METHOD2(GetLogins, int(const webkit::forms::PasswordForm&,
                              PasswordStoreConsumer*));
  MOCK_METHOD1(AddLogin, void(const webkit::forms::PasswordForm&));
  MOCK_METHOD1(UpdateLogin, void(const webkit::forms::PasswordForm&));
  MOCK_METHOD0(ReportMetrics, void());
  MOCK_METHOD0(ReportMetricsImpl, void());
  MOCK_METHOD1(AddLoginImpl, void(const webkit::forms::PasswordForm&));
  MOCK_METHOD1(UpdateLoginImpl, void(const webkit::forms::PasswordForm&));
  MOCK_METHOD1(RemoveLoginImpl, void(const webkit::forms::PasswordForm&));
  MOCK_METHOD2(RemoveLoginsCreatedBetweenImpl, void(const base::Time&,
               const base::Time&));
  MOCK_METHOD2(GetLoginsImpl, void(GetLoginsRequest*,
                                   const webkit::forms::PasswordForm&));
  MOCK_METHOD1(GetAutofillableLoginsImpl, void(GetLoginsRequest*));
  MOCK_METHOD1(GetBlacklistLoginsImpl, void(GetLoginsRequest*));
  MOCK_METHOD1(FillAutofillableLogins,
      bool(std::vector<webkit::forms::PasswordForm*>*));
  MOCK_METHOD1(FillBlacklistLogins,
      bool(std::vector<webkit::forms::PasswordForm*>*));

  virtual void ShutdownOnUIThread();

 private:
  virtual ~MockPasswordStore();
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_H_
