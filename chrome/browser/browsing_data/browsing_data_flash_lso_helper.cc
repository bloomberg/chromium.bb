// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"

#include <limits>
#include <map>

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/pepper_flash_settings_manager.h"

namespace {

class BrowsingDataFlashLSOHelperImpl
    : public BrowsingDataFlashLSOHelper,
      public PepperFlashSettingsManager::Client {
 public:
  explicit BrowsingDataFlashLSOHelperImpl(
      content::BrowserContext* browser_context);

  // BrowsingDataFlashLSOHelper implementation:
  virtual void StartFetching(
      const GetSitesWithFlashDataCallback& callback) OVERRIDE;
  virtual void DeleteFlashLSOsForSite(const std::string& site) OVERRIDE;

  // PepperFlashSettingsManager::Client overrides:
  virtual void OnGetSitesWithDataCompleted(
      uint32 request_id,
      const std::vector<std::string>& sites) OVERRIDE;
  virtual void OnClearSiteDataCompleted(
      uint32 request_id,
      bool success) OVERRIDE;

 private:
  virtual ~BrowsingDataFlashLSOHelperImpl();

  // Asynchronously fetches and deletes data and calls us back.
  PepperFlashSettingsManager settings_manager_;

  // Identifies the request to fetch site data.
  uint32 get_sites_with_data_request_id_;

  // Contains the pending requests to clear site data. The key is the request
  // ID, the value the site for which to clear data.
  std::map<uint32, std::string> clear_site_data_ids_;

  // Called when we have fetched the list of sites.
  GetSitesWithFlashDataCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataFlashLSOHelperImpl);
};

BrowsingDataFlashLSOHelperImpl::BrowsingDataFlashLSOHelperImpl(
    content::BrowserContext* browser_context)
    : settings_manager_(this, browser_context),
      get_sites_with_data_request_id_(0u) {
}

BrowsingDataFlashLSOHelperImpl::~BrowsingDataFlashLSOHelperImpl() {
}

void BrowsingDataFlashLSOHelperImpl::StartFetching(
    const GetSitesWithFlashDataCallback& callback) {
  DCHECK(callback_.is_null());
  callback_ = callback;
  get_sites_with_data_request_id_ = settings_manager_.GetSitesWithData();
}

void BrowsingDataFlashLSOHelperImpl::DeleteFlashLSOsForSite(
    const std::string& site) {
  const uint64 kClearAllData = 0;
  uint32 id = settings_manager_.ClearSiteData(
      site, kClearAllData, std::numeric_limits<uint64>::max());
  clear_site_data_ids_[id] = site;
}

void BrowsingDataFlashLSOHelperImpl::OnGetSitesWithDataCompleted(
    uint32 request_id,
    const std::vector<std::string>& sites) {
  DCHECK_EQ(get_sites_with_data_request_id_, request_id);
  callback_.Run(sites);
  callback_ = GetSitesWithFlashDataCallback();
}

void BrowsingDataFlashLSOHelperImpl::OnClearSiteDataCompleted(uint32 request_id,
                                                              bool success) {
  std::map<uint32, std::string>::iterator entry =
      clear_site_data_ids_.find(request_id);
  DCHECK(entry != clear_site_data_ids_.end());
  LOG_IF(ERROR, !success) << "Couldn't clear Flash LSO data for "
                          << entry->second;
  clear_site_data_ids_.erase(entry);
}

}  // namespace

// static
BrowsingDataFlashLSOHelper* BrowsingDataFlashLSOHelper::Create(
    content::BrowserContext* browser_context) {
  return new BrowsingDataFlashLSOHelperImpl(browser_context);
}
