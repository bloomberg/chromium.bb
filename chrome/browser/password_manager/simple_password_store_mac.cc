// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/simple_password_store_mac.h"

SimplePasswordStoreMac::SimplePasswordStoreMac(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_refptr<base::SingleThreadTaskRunner> background_thread_runner,
    scoped_ptr<password_manager::LoginDatabase> login_db)
    : PasswordStoreDefault(main_thread_runner, background_thread_runner,
                           login_db.Pass()) {
  this->login_db()->set_clear_password_values(false);
}

SimplePasswordStoreMac::~SimplePasswordStoreMac() {
}

bool SimplePasswordStoreMac::Init(
    const syncer::SyncableService::StartSyncFlare& flare) {
  // All the initialization has to be done by the owner of the object.
  return true;
}

void SimplePasswordStoreMac::Shutdown() {
  PasswordStoreDefault::Shutdown();
}
