// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
#pragma once

#include <list>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace content {
class DOMStorageContext;
}

// This class fetches local storage information in the WebKit thread, and
// notifies the UI thread upon completion.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI
// thread at some later point.
class BrowsingDataLocalStorageHelper
    : public base::RefCountedThreadSafe<BrowsingDataLocalStorageHelper> {
 public:
  // Contains detailed information about local storage.
  struct LocalStorageInfo {
    LocalStorageInfo(
        const std::string& protocol,
        const std::string& host,
        unsigned short port,
        const std::string& database_identifier,
        const std::string& origin,
        const FilePath& file_path,
        int64 size,
        base::Time last_modified);
    ~LocalStorageInfo();

    std::string protocol;
    std::string host;
    unsigned short port;
    std::string database_identifier;
    std::string origin;
    FilePath file_path;
    int64 size;
    base::Time last_modified;
  };

  explicit BrowsingDataLocalStorageHelper(Profile* profile);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      const base::Callback<void(const std::list<LocalStorageInfo>&)>& callback);
  // Requests a single local storage file to be deleted in the WEBKIT thread.
  virtual void DeleteLocalStorageFile(const FilePath& file_path);

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataLocalStorageHelper>;
  virtual ~BrowsingDataLocalStorageHelper();

  // Notifies the completion callback in the UI thread.
  void NotifyInUIThread();

  // Owned by the profile
  content::DOMStorageContext* dom_storage_context_;

  // This only mutates on the UI thread.
  base::Callback<void(const std::list<LocalStorageInfo>&)> completion_callback_;

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notified the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

  // Access to |local_storage_info_| is triggered indirectly via the UI thread
  // and guarded by |is_fetching_|. This means |local_storage_info_| is only
  // accessed while |is_fetching_| is true. The flag |is_fetching_| is only
  // accessed on the UI thread.
  std::list<LocalStorageInfo> local_storage_info_;

 private:
  // Called back with all the local storage files.
  void GetAllStorageFilesCallback(const std::vector<FilePath>& files);
  // Get the file info on the file thread.
  void FetchLocalStorageInfo(const std::vector<FilePath>& files);

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataLocalStorageHelper);
};

// This class is a thin wrapper around BrowsingDataLocalStorageHelper that does
// not fetch its information from the local storage tracker, but gets them
// passed as a parameter during construction.
class CannedBrowsingDataLocalStorageHelper
    : public BrowsingDataLocalStorageHelper {
 public:
  explicit CannedBrowsingDataLocalStorageHelper(Profile* profile);

  // Return a copy of the local storage helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // every time we instantiate a cookies tree model for it.
  CannedBrowsingDataLocalStorageHelper* Clone();

  // Add a local storage to the set of canned local storages that is returned
  // by this helper.
  void AddLocalStorage(const GURL& origin);

  // Clear the list of canned local storages.
  void Reset();

  // True if no local storages are currently stored.
  bool empty() const;

  // Returns the number of local storages currently stored.
  size_t GetLocalStorageCount() const;

  // Returns the set of origins that use local storage.
  const std::set<GURL>& GetLocalStorageInfo() const;

  // BrowsingDataLocalStorageHelper implementation.
  virtual void StartFetching(
      const base::Callback<void(const std::list<LocalStorageInfo>&)>& callback)
          OVERRIDE;

 private:
  virtual ~CannedBrowsingDataLocalStorageHelper();

  // Convert the pending local storage info to local storage info objects.
  void ConvertPendingInfo();

  std::set<GURL> pending_local_storage_info_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataLocalStorageHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
