// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_

#include <string>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/task.h"

class Profile;

// This class fetches local storage information in the WebKit thread, and
// notifies the UI thread upon completion.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI
// thread at some later point.
// The client must call CancelNotification() if it's destroyed before the
// callback is notified.
class BrowsingDataLocalStorageHelper
    : public base::RefCountedThreadSafe<BrowsingDataLocalStorageHelper> {
 public:
  // Contains detailed information about local storage.
  struct LocalStorageInfo {
    LocalStorageInfo() {}
    LocalStorageInfo(
        const std::string& protocol,
        const std::string& host,
        unsigned short port,
        const std::string& database_identifier,
        const std::string& origin,
        const FilePath& file_path,
        int64 size,
        base::Time last_modified)
        : protocol(protocol),
          host(host),
          port(port),
          database_identifier(database_identifier),
          origin(origin),
          file_path(file_path),
          size(size),
          last_modified(last_modified) {
    }

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
      Callback1<const std::vector<LocalStorageInfo>& >::Type* callback);
  // Cancels the notification callback (i.e., the window that created it no
  // longer exists).
  // This must be called only in the UI thread.
  virtual void CancelNotification();
  // Requests a single local storage file to be deleted in the WEBKIT thread.
  virtual void DeleteLocalStorageFile(const FilePath& file_path);
  // Requests all local storage files to be deleted in the WEBKIT thread.
  virtual void DeleteAllLocalStorageFiles();

 private:
  friend class base::RefCountedThreadSafe<BrowsingDataLocalStorageHelper>;
  friend class MockBrowsingDataLocalStorageHelper;
  virtual ~BrowsingDataLocalStorageHelper();

  // Enumerates all local storage files in the WEBKIT thread.
  void FetchLocalStorageInfoInWebKitThread();
  // Notifies the completion callback in the UI thread.
  void NotifyInUIThread();
  // Delete a single local storage file in the WEBKIT thread.
  void DeleteLocalStorageFileInWebKitThread(const FilePath& file_path);
  // Delete all local storage files in the WEBKIT thread.
  void DeleteAllLocalStorageFilesInWebKitThread();

  Profile* profile_;
  // This only mutates on the UI thread.
  scoped_ptr<Callback1<const std::vector<LocalStorageInfo>& >::Type >
      completion_callback_;
  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notified the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;
  // This only mutates in the WEBKIT thread.
  std::vector<LocalStorageInfo> local_storage_info_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataLocalStorageHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
