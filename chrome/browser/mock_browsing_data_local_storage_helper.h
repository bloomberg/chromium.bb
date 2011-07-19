// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MOCK_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
#define CHROME_BROWSER_MOCK_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
#pragma once

#include <map>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"

// Mock for BrowsingDataLocalStorageHelper.
// Use AddLocalStorageSamples() or add directly to response_ vector, then
// call Notify().
class MockBrowsingDataLocalStorageHelper
    : public BrowsingDataLocalStorageHelper {
 public:
  explicit MockBrowsingDataLocalStorageHelper(Profile* profile);

  virtual void StartFetching(
      Callback1<const std::vector<LocalStorageInfo>& >::Type* callback);

  virtual void CancelNotification();

  virtual void DeleteLocalStorageFile(const FilePath& file_path);

  // Adds some LocalStorageInfo samples.
  void AddLocalStorageSamples();

  // Notifies the callback.
  void Notify();

  // Marks all local storage files as existing.
  void Reset();

  // Returns true if all local storage files were deleted since the last
  // Reset() invokation.
  bool AllDeleted();

  FilePath last_deleted_file_;

 private:
  virtual ~MockBrowsingDataLocalStorageHelper();

  Profile* profile_;

  scoped_ptr<Callback1<const std::vector<LocalStorageInfo>& >::Type >
      callback_;

  std::map<const FilePath::StringType, bool> files_;

  std::vector<LocalStorageInfo> response_;
};

#endif  // CHROME_BROWSER_MOCK_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
