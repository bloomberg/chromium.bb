// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_SERVICE_H_

#include "base/memory/scoped_ptr.h"
#include "components/password_manager/core/browser/password_store_service.h"

namespace content {
class BrowserContext;
}

class MockPasswordStoreService : public password_manager::PasswordStoreService {
 public:
  static scoped_ptr<KeyedService> Build(content::BrowserContext* profile);

 private:
  explicit MockPasswordStoreService(
      scoped_refptr<password_manager::PasswordStore> password_store);

  ~MockPasswordStoreService() override;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordStoreService);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_SERVICE_H_
