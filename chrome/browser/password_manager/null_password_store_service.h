// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NULL_PASSWORD_STORE_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NULL_PASSWORD_STORE_SERVICE_H_

#include "chrome/browser/password_manager/password_store_factory.h"

class PasswordStore;

namespace content {
class BrowserContext;
}

class NullPasswordStoreService : public PasswordStoreService {
 public:
  static KeyedService* Build(content::BrowserContext* profile);

 private:
  NullPasswordStoreService();

  virtual ~NullPasswordStoreService();

  DISALLOW_COPY_AND_ASSIGN(NullPasswordStoreService);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_NULL_PASSWORD_STORE_SERVICE_H_
