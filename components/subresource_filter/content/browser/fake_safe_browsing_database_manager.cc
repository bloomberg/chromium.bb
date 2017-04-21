// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

FakeSafeBrowsingDatabaseManager::FakeSafeBrowsingDatabaseManager()
    : simulate_timeout_(false) {}

void FakeSafeBrowsingDatabaseManager::AddBlacklistedUrl(
    const GURL& url,
    safe_browsing::SBThreatType threat_type) {
  url_to_threat_type_[url] = threat_type;
}

void FakeSafeBrowsingDatabaseManager::SimulateTimeout() {
  simulate_timeout_ = true;
}

FakeSafeBrowsingDatabaseManager::~FakeSafeBrowsingDatabaseManager() {}

bool FakeSafeBrowsingDatabaseManager::CheckUrlForSubresourceFilter(
    const GURL& url,
    Client* client) {
  if (simulate_timeout_)
    return false;
  if (!url_to_threat_type_.count(url))
    return true;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&Client::OnCheckBrowseUrlResult, base::Unretained(client), url,
                 url_to_threat_type_[url], safe_browsing::ThreatMetadata()));
  return false;
}

bool FakeSafeBrowsingDatabaseManager::CheckResourceUrl(const GURL& url,
                                                       Client* client) {
  return true;
}

bool FakeSafeBrowsingDatabaseManager::IsSupported() const {
  return true;
}
bool FakeSafeBrowsingDatabaseManager::ChecksAreAlwaysAsync() const {
  return false;
}
void FakeSafeBrowsingDatabaseManager::CancelCheck(Client* client) {}
bool FakeSafeBrowsingDatabaseManager::CanCheckResourceType(
    content::ResourceType /* resource_type */) const {
  return true;
}

safe_browsing::ThreatSource FakeSafeBrowsingDatabaseManager::GetThreatSource()
    const {
  return safe_browsing::ThreatSource::LOCAL_PVER4;
}

bool FakeSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  return true;
}
