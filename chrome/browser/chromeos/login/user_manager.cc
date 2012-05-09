// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager.h"

#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

// static
const char UserManager::kLoggedInUsers[] = "LoggedInUsers";
const char UserManager::kUserWallpapers[] = "UserWallpapers";
const char UserManager::kUserWallpapersProperties[] =
    "UserWallpapersProperties";
const char UserManager::kUserImages[] = "UserImages";
const char UserManager::kUserDisplayEmail[] = "UserDisplayEmail";
const char UserManager::kUserOAuthTokenStatus[] = "OAuthTokenStatus";

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
    base::AutoLock create(create_lock_);
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

  base::Lock create_lock_;
  scoped_ptr<UserManager> ptr_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerImplWrapper);
};

// static
UserManager* UserManager::Get() {
  // UserManager instance should be used only on UI thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return UserManagerImplWrapper::GetInstance()->get();
}

// static
UserManager* UserManager::Set(UserManager* mock) {
  UserManager* old_manager = UserManagerImplWrapper::GetInstance()->release();
  UserManagerImplWrapper::GetInstance()->reset(mock);
  return old_manager;
}

// static
void UserManager::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterListPref(kLoggedInUsers, PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserWallpapers,
                                      PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserWallpapersProperties,
                                      PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserImages,
                                      PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserOAuthTokenStatus,
                                      PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserDisplayEmail,
                                      PrefService::UNSYNCABLE_PREF);
}

UserManager::~UserManager() {
}

}  // namespace chromeos
