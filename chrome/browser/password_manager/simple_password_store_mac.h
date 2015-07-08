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
  SimplePasswordStoreMac(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SingleThreadTaskRunner> background_thread_runner,
      scoped_ptr<password_manager::LoginDatabase> login_db);

  // Just hides the parent method. All the initialization is to be done by
  // PasswordStoreProxyMac that is an owner of the class.
  bool Init(const syncer::SyncableService::StartSyncFlare& flare) override;

  // Clean |background_thread_runner_|.
  void Shutdown() override;

  using PasswordStoreDefault::GetBackgroundTaskRunner;

 protected:
  ~SimplePasswordStoreMac() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimplePasswordStoreMac);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_SIMPLE_PASSWORD_STORE_MAC_H_
