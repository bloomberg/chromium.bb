// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/null_password_store_service.h"

#include "components/password_manager/core/browser/password_store.h"

// static
KeyedService* NullPasswordStoreService::Build(
    content::BrowserContext* /*profile*/) {
  return new NullPasswordStoreService();
}

NullPasswordStoreService::NullPasswordStoreService()
    : PasswordStoreService(NULL) {}

NullPasswordStoreService::~NullPasswordStoreService() {}
