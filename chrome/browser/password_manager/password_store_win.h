// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
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
  // The |login_db| must not have been Init()-ed yet. It will be initialized in
  // a deferred manner on the DB thread. The |web_data_service| is only used for
  // IE7 password fetching.
  PasswordStoreWin(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
      scoped_ptr<password_manager::LoginDatabase> login_db,
      const scoped_refptr<PasswordWebDataService>& web_data_service);

  // PasswordStore:
  virtual void Shutdown() override;

 private:
  class DBHandler;

  virtual ~PasswordStoreWin();

  // Invoked from Shutdown, but run on the DB thread.
  void ShutdownOnDBThread();

  // password_manager::PasswordStore:
  void GetLoginsImpl(const autofill::PasswordForm& form,
                     AuthorizationPromptPolicy prompt_policy,
                     scoped_ptr<GetLoginsRequest> request) override;

  scoped_ptr<DBHandler> db_handler_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreWin);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_
