// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_service.h"

namespace password_manager {

PasswordStoreService::PasswordStoreService(
    scoped_refptr<PasswordStore> password_store)
    : password_store_(password_store) {}

PasswordStoreService::~PasswordStoreService() {}

scoped_refptr<PasswordStore> PasswordStoreService::GetPasswordStore() {
  return password_store_;
}

void PasswordStoreService::Shutdown() {
  if (password_store_)
    password_store_->Shutdown();
}

}  // namespace password_manager
