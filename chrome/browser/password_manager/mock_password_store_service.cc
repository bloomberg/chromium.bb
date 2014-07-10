// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/mock_password_store_service.h"

#include "components/password_manager/core/browser/mock_password_store.h"

// static
KeyedService* MockPasswordStoreService::Build(
    content::BrowserContext* /*profile*/) {
  scoped_refptr<password_manager::PasswordStore> store(
      new password_manager::MockPasswordStore);
  if (!store || !store->Init(syncer::SyncableService::StartSyncFlare(), ""))
    return NULL;
  return new MockPasswordStoreService(store);
}

MockPasswordStoreService::MockPasswordStoreService(
    scoped_refptr<password_manager::PasswordStore> password_store)
    : PasswordStoreService(password_store) {}

MockPasswordStoreService::~MockPasswordStoreService() {}
