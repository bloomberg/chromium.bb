// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_OBSERVER_H_

#include "base/string16.h"
#include "ui/gfx/image/image.h"

namespace base {
class FilePath;
}

// This class provides an Observer interface to watch for changes to the
// ProfileInfoCache.
class ProfileInfoCacheObserver {
 public:
  virtual ~ProfileInfoCacheObserver() {}

  virtual void OnProfileAdded(const base::FilePath& profile_path) = 0;
  virtual void OnProfileWillBeRemoved(const base::FilePath& profile_path) = 0;
  virtual void OnProfileWasRemoved(const base::FilePath& profile_path,
                                   const string16& profile_name) = 0;
  virtual void OnProfileNameChanged(const base::FilePath& profile_path,
                                    const string16& old_profile_name) = 0;
  virtual void OnProfileAvatarChanged(const base::FilePath& profile_path) = 0;

 protected:
  ProfileInfoCacheObserver() {}

  DISALLOW_COPY_AND_ASSIGN(ProfileInfoCacheObserver);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_OBSERVER_H_
