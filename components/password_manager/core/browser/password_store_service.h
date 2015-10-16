// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

// A wrapper of PasswordStore so we can use it as a profiled keyed service. This
// is necessary, because while the service has a signle owner (its factory), the
// store is refcounted.
class PasswordStoreService : public KeyedService {
 public:
  // |password_store| needs to be not-NULL, and the constructor expects that
  // Init() was already called successfully on it.
  explicit PasswordStoreService(scoped_refptr<PasswordStore> password_store);
  ~PasswordStoreService() override;

  scoped_refptr<PasswordStore> GetPasswordStore();

  // KeyedService implementation.
  void Shutdown() override;

 private:
  scoped_refptr<PasswordStore> password_store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreService);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SERVICE_H_
