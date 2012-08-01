// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_FLASH_LSO_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_FLASH_LSO_HELPER_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"

class MockBrowsingDataFlashLSOHelper : public BrowsingDataFlashLSOHelper {
 public:
  explicit MockBrowsingDataFlashLSOHelper(
      content::BrowserContext* browser_context);

  // BrowsingDataFlashLSOHelper implementation:
  virtual void StartFetching(
      const GetSitesWithFlashDataCallback& callback) OVERRIDE;
  virtual void DeleteFlashLSOsForSite(const std::string& site) OVERRIDE;

  // Adds a domain sample.
  void AddFlashLSODomain(const std::string& domain);

  // Notifies the callback.
  void Notify();

  // Returns true if the domain list is empty.
  bool AllDeleted();

 private:
  virtual ~MockBrowsingDataFlashLSOHelper();

  GetSitesWithFlashDataCallback callback_;

  std::vector<std::string> domains_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_FLASH_LSO_HELPER_H_
