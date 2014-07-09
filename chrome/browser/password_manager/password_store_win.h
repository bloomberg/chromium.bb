// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_

#include "base/memory/scoped_ptr.h"
#include "components/password_manager/core/browser/password_store_default.h"

class PasswordWebDataService;

namespace autofill {
struct PasswordForm;
}

namespace password_manager {
class LoginDatabase;
}

// Windows PasswordStore implementation that uses the default implementation,
// but also uses IE7 passwords if no others found.
class PasswordStoreWin : public password_manager::PasswordStoreDefault {
 public:
  // PasswordWebDataService is only used for IE7 password fetching.
  PasswordStoreWin(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
      password_manager::LoginDatabase* login_database,
      PasswordWebDataService* web_data_service);

  // PasswordStore:
  virtual void Shutdown() OVERRIDE;

 private:
  class DBHandler;

  virtual ~PasswordStoreWin();

  // Invoked from Shutdown, but run on the DB thread.
  void ShutdownOnDBThread();

  virtual void GetLoginsImpl(
      const autofill::PasswordForm& form,
      AuthorizationPromptPolicy prompt_policy,
      const ConsumerCallbackRunner& callback_runner) OVERRIDE;

  void GetIE7LoginIfNecessary(
    const autofill::PasswordForm& form,
    const ConsumerCallbackRunner& callback_runner,
    const std::vector<autofill::PasswordForm*>& matched_forms);

  scoped_ptr<DBHandler> db_handler_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreWin);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_
