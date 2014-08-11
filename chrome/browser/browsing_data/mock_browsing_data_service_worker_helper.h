// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_SERVICE_WORKER_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_SERVICE_WORKER_HELPER_H_

#include <list>
#include <map>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"

class Profile;

// Mock for BrowsingDataServiceWorkerHelper.
// Use AddServiceWorkerSamples() or add directly to response_ list, then
// call Notify().
class MockBrowsingDataServiceWorkerHelper
    : public BrowsingDataServiceWorkerHelper {
 public:
  explicit MockBrowsingDataServiceWorkerHelper(Profile* profile);

  // Adds some ServiceWorkerInfo samples.
  void AddServiceWorkerSamples();

  // Notifies the callback.
  void Notify();

  // Marks all service worker files as existing.
  void Reset();

  // Returns true if all service worker files were deleted since the last
  // Reset() invokation.
  bool AllDeleted();

  // BrowsingDataServiceWorkerHelper.
  virtual void StartFetching(const base::Callback<void(
      const std::list<content::ServiceWorkerUsageInfo>&)>& callback) OVERRIDE;
  virtual void DeleteServiceWorkers(const GURL& origin) OVERRIDE;

 private:
  virtual ~MockBrowsingDataServiceWorkerHelper();

  base::Callback<void(const std::list<content::ServiceWorkerUsageInfo>&)>
      callback_;
  std::map<GURL, bool> origins_;
  std::list<content::ServiceWorkerUsageInfo> response_;

  DISALLOW_COPY_AND_ASSIGN(MockBrowsingDataServiceWorkerHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_SERVICE_WORKER_HELPER_H_
