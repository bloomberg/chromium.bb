// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_TEST_PASSWORD_STORE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_TEST_PASSWORD_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/password_manager/password_store.h"

namespace content {
class BrowserContext;
}

scoped_refptr<RefcountedBrowserContextKeyedService> CreateTestPasswordStore(
    content::BrowserContext* profile);

// A very simple PasswordStore implementation that keeps all of the passwords
// in memory and does all it's manipulations on the main thread. Since this
// is only used for testing, only the parts of the interface that are needed
// for testing have been implemented.
class TestPasswordStore : public PasswordStore {
 public:
  TestPasswordStore();

  // Helper function for registration with
  // RefcountedBrowserContextKeyedService::SetTestingFactory
  static scoped_refptr<RefcountedBrowserContextKeyedService> Create(
      content::BrowserContext* profile);

  typedef std::map<std::string /* signon_realm */,
                   std::vector<autofill::PasswordForm> > PasswordMap;

  PasswordMap stored_passwords();
  void Clear();

 protected:
  virtual ~TestPasswordStore();

  // Helper function to determine if forms are considered equivalent.
  bool FormsAreEquivalent(const autofill::PasswordForm& lhs,
                          const autofill::PasswordForm& rhs);

  // PasswordStore interface
  virtual void AddLoginImpl(const autofill::PasswordForm& form) OVERRIDE;
  virtual void UpdateLoginImpl(const autofill::PasswordForm& form) OVERRIDE;
  virtual void RemoveLoginImpl(const autofill::PasswordForm& form) OVERRIDE;
  virtual void GetLoginsImpl(const autofill::PasswordForm& form,
                             const ConsumerCallbackRunner& runner) OVERRIDE;
  virtual bool ScheduleTask(const base::Closure& task) OVERRIDE;
  virtual void WrapModificationTask(base::Closure task) OVERRIDE;

  // Unused portions of PasswordStore interface
  virtual void ReportMetricsImpl() OVERRIDE {}
  virtual void RemoveLoginsCreatedBetweenImpl(const base::Time& begin,
                                              const base::Time& end) OVERRIDE {}
  virtual void GetAutofillableLoginsImpl(
      PasswordStore::GetLoginsRequest* request) OVERRIDE {}
  virtual void GetBlacklistLoginsImpl(
      PasswordStore::GetLoginsRequest* request) OVERRIDE {}
  virtual bool FillAutofillableLogins(
      std::vector<autofill::PasswordForm*>* forms) OVERRIDE;
  virtual bool FillBlacklistLogins(
      std::vector<autofill::PasswordForm*>* forms) OVERRIDE;
  virtual void ShutdownOnUIThread() OVERRIDE {}

 private:
  PasswordMap stored_passwords_;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordStore);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_TEST_PASSWORD_STORE_H_
