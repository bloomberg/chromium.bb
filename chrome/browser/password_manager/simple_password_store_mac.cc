// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/simple_password_store_mac.h"

#include <utility>

SimplePasswordStoreMac::SimplePasswordStoreMac(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_refptr<base::SingleThreadTaskRunner> background_thread_runner,
    std::unique_ptr<password_manager::LoginDatabase> login_db)
    : PasswordStoreDefault(main_thread_runner,
                           background_thread_runner,
                           std::move(login_db)) {
  if (this->login_db())
    this->login_db()->set_clear_password_values(false);
}

SimplePasswordStoreMac::~SimplePasswordStoreMac() {
}

void SimplePasswordStoreMac::InitWithTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner,
    std::unique_ptr<password_manager::LoginDatabase> login_db) {
  db_thread_runner_ = background_task_runner;
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  set_login_db(std::move(login_db));
  if (this->login_db())
    this->login_db()->set_clear_password_values(false);
}

bool SimplePasswordStoreMac::Init(
    const syncer::SyncableService::StartSyncFlare& flare) {
  NOTREACHED();
  return false;
}
