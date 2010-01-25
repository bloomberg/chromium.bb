// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MOCK_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
#define CHROME_BROWSER_MOCK_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_

#include "chrome/browser/browsing_data_local_storage_helper.h"

// Mock for BrowsingDataLocalStorageHelper.
// Use AddLocalStorageSamples() or add directly to response_ vector, then
// call Notify().
class MockBrowsingDataLocalStorageHelper
    : public BrowsingDataLocalStorageHelper {
 public:
  explicit MockBrowsingDataLocalStorageHelper(Profile* profile);
  virtual ~MockBrowsingDataLocalStorageHelper();

  virtual void StartFetching(
      Callback1<const std::vector<LocalStorageInfo>& >::Type* callback);
  virtual void CancelNotification();
  virtual void DeleteLocalStorageFile(const FilePath& file_path);
  virtual void DeleteAllLocalStorageFiles();

  // Adds some LocalStorageInfo samples.
  void AddLocalStorageSamples();

  // Notifies the callback.
  void Notify();

  Profile* profile_;
  scoped_ptr<Callback1<const std::vector<LocalStorageInfo>& >::Type >
      callback_;
  FilePath last_deleted_file_;
  bool delete_all_files_called_;
  std::vector<LocalStorageInfo> response_;
};

#endif  // CHROME_BROWSER_MOCK_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
