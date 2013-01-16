// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager.h"

#include "chrome/browser/chromeos/login/user_manager_impl.h"

namespace chromeos {

// static
const char UserManager::kStubUser[] = "stub-user@example.com";

// static
const char UserManager::kLocallyManagedUserDomain[] =
    "locally-managed.localhost";

// Class that is holds pointer to UserManager instance.
// One could set UserManager mock instance through it (see UserManager::Set).
class UserManagerImplWrapper {
 public:
  static UserManagerImplWrapper* GetInstance() {
    return Singleton<UserManagerImplWrapper>::get();
  }

 protected:
  ~UserManagerImplWrapper() {
  }

  UserManager* get() {
    if (!ptr_.get())
      reset(new UserManagerImpl);
    return ptr_.get();
  }

  void reset(UserManager* ptr) {
    ptr_.reset(ptr);
  }

  UserManager* release() {
    return ptr_.release();
  }

 private:
  friend struct DefaultSingletonTraits<UserManagerImplWrapper>;

  friend class UserManager;

  UserManagerImplWrapper() {
  }

  scoped_ptr<UserManager> ptr_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerImplWrapper);
};

// static
UserManager* UserManager::Get() {
  return UserManagerImplWrapper::GetInstance()->get();
}

// static
UserManager* UserManager::Set(UserManager* mock) {
  UserManager* old_manager = UserManagerImplWrapper::GetInstance()->release();
  UserManagerImplWrapper::GetInstance()->reset(mock);
  return old_manager;
}

UserManager::~UserManager() {
}

}  // namespace chromeos
