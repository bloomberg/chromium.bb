// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_SIMPLE_PASSWORD_STORE_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_SIMPLE_PASSWORD_STORE_MAC_H_

#include "components/password_manager/core/browser/password_store_default.h"

// The same as PasswordStoreDefault but running on the dedicated thread. The
// owner is responsible for the thread lifetime.
class SimplePasswordStoreMac : public password_manager::PasswordStoreDefault {
 public:
  // Passes the arguments to PasswordStoreDefault. |background_task_runner| and
  // |login_db| can be overwritten later in InitWithTaskRunner().
  SimplePasswordStoreMac(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SingleThreadTaskRunner> background_thread_runner,
      scoped_ptr<password_manager::LoginDatabase> login_db);

  // Sets the background thread and LoginDatabase. |login_db| should be already
  // inited. The method isn't necessary to call if the constructor already got
  // the correct parameters.
  void InitWithTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner,
      scoped_ptr<password_manager::LoginDatabase> login_db);

  using PasswordStoreDefault::GetBackgroundTaskRunner;

 protected:
  ~SimplePasswordStoreMac() override;

 private:
  bool Init(const syncer::SyncableService::StartSyncFlare& flare) override;

  DISALLOW_COPY_AND_ASSIGN(SimplePasswordStoreMac);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_SIMPLE_PASSWORD_STORE_MAC_H_
