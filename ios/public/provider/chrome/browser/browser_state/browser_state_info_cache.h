// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"

namespace base {
class FilePath;
}

namespace ios {

class BrowserStateInfoCacheObserver;

// This class saves various information about browser states to local
// preferences.
class BrowserStateInfoCache {
 public:
  BrowserStateInfoCache();
  virtual ~BrowserStateInfoCache();

  // Adds and removes an observer.
  void AddObserver(BrowserStateInfoCacheObserver* observer);
  void RemoveObserver(BrowserStateInfoCacheObserver* observer);

  // Gets and sets information related to browser states.
  virtual size_t GetIndexOfBrowserStateWithPath(
      const base::FilePath& profile_path) const = 0;
  virtual void SetLocalAuthCredentialsOfBrowserStateAtIndex(
      size_t index,
      const std::string& credentials) = 0;
  virtual void SetAuthInfoOfBrowserStateAtIndex(
      size_t index,
      const std::string& gaia_id,
      const base::string16& user_name) = 0;
  virtual void SetBrowserStateSigninRequiredAtIndex(size_t index,
                                                    bool value) = 0;
  virtual void SetBrowserStateIsAuthErrorAtIndex(size_t index, bool value) = 0;
  virtual std::string GetPasswordChangeDetectionTokenAtIndex(
      size_t index) const = 0;
  virtual void SetPasswordChangeDetectionTokenAtIndex(
      size_t index,
      const std::string& token) = 0;

 protected:
  // Methods calling the observers in |observer_list_|.
  void NotifyBrowserStateAdded(const base::FilePath& path);
  void NotifyBrowserStateRemoved(const base::FilePath& profile_path,
                                 const base::string16& name);

 private:
  base::ObserverList<BrowserStateInfoCacheObserver, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(BrowserStateInfoCache);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_INFO_CACHE_H_
