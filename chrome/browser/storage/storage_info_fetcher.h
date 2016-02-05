// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_STORAGE_INFO_FETCHER_H_
#define CHROME_BROWSER_STORAGE_STORAGE_INFO_FETCHER_H_

#include "base/memory/ref_counted.h"
#include "storage/browser/quota/quota_manager.h"

// Asynchronously fetches the amount of storage used by websites.
class StorageInfoFetcher :
    public base::RefCountedThreadSafe<StorageInfoFetcher> {
 public:
  // Observer interface for monitoring StorageInfoFetcher.
  class Observer {
   public:
    // Called when the storage has been calculated.
    virtual void OnGetUsageInfo(const storage::UsageInfoEntries& entries) = 0;

    // Called when the storage has been cleared.
    virtual void OnUsageInfoCleared(storage::QuotaStatusCode code) = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit StorageInfoFetcher(storage::QuotaManager* quota_manager);

  // Asynchronously fetches the StorageInfo.
  void FetchStorageInfo();

  // Asynchronously clears storage for the given host.
  void ClearStorage(const std::string& host, storage::StorageType type);

  // Called when usage has been cleared.
  void OnUsageCleared(storage::QuotaStatusCode code);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  virtual ~StorageInfoFetcher();

  friend class base::RefCountedThreadSafe<StorageInfoFetcher>;

  // Fetches the usage information.
  void GetUsageInfo(const storage::GetUsageInfoCallback& callback);

  // Called when usage information is available.
  void OnGetUsageInfoInternal(const storage::UsageInfoEntries& entries);

  // Reports back to all observers that information is available.
  void InvokeCallback();

  // All clients observing this class.
  base::ObserverList<Observer> observers_;

  // The quota manager to use to calculate the storage usage.
  storage::QuotaManager* quota_manager_;

  // Hosts and their usage.
  storage::UsageInfoEntries entries_;

  DISALLOW_COPY_AND_ASSIGN(StorageInfoFetcher);
};

#endif  // CHROME_BROWSER_STORAGE_STORAGE_INFO_FETCHER_H_
