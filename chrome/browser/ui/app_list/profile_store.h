// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_PROFILE_STORE_H_
#define CHROME_BROWSER_UI_APP_LIST_PROFILE_STORE_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"

class Profile;

// Represents something that knows how to load profiles asynchronously.
class ProfileStore {
 public:
  virtual ~ProfileStore() {}
  virtual void AddProfileObserver(
      ProfileAttributesStorage::Observer* observer) = 0;

  // Loads the profile at |path| and calls |callback| when its done. A NULL
  // Profile* represents an error.
  virtual void LoadProfileAsync(const base::FilePath& path,
                                base::Callback<void(Profile*)> callback) = 0;

  // Returns the profile at |path| if it is already loaded.
  virtual Profile* GetProfileByPath(const base::FilePath& path) = 0;

  // The user data directory that profiles are stored under in this instance of
  // Chrome.
  virtual base::FilePath GetUserDataDir() = 0;

  // The name of the last used profile.
  virtual std::string GetLastUsedProfileName() = 0;

  // Returns true if the profile at |path| is supervised.
  virtual bool IsProfileSupervised(const base::FilePath& path) = 0;

  // Returns true if the profile at |path| is locked.
  virtual bool IsProfileLocked(const base::FilePath& path) = 0;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_PROFILE_STORE_H_
