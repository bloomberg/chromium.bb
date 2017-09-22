// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/test_database_manager.h"

#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "net/url_request/url_request_context_getter.h"

namespace safe_browsing {

void TestSafeBrowsingDatabaseManager::CancelCheck(Client* client) {
  NOTIMPLEMENTED();
}

bool TestSafeBrowsingDatabaseManager::CanCheckResourceType(
    content::ResourceType resource_type) const {
  NOTIMPLEMENTED();
  return false;
}

bool TestSafeBrowsingDatabaseManager::CanCheckSubresourceFilter() const {
  return false;
}

bool TestSafeBrowsingDatabaseManager::CanCheckUrl(const GURL& url) const {
  NOTIMPLEMENTED();
  return false;
}

bool TestSafeBrowsingDatabaseManager::ChecksAreAlwaysAsync() const {
  NOTIMPLEMENTED();
  return false;
}

bool TestSafeBrowsingDatabaseManager::CheckBrowseUrl(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::CheckResourceUrl(const GURL& url,
                                                       Client* client) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::CheckUrlForSubresourceFilter(
    const GURL& url,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

AsyncMatch TestSafeBrowsingDatabaseManager::CheckCsdWhitelistUrl(
    const GURL& url,
    Client* client) {
  NOTIMPLEMENTED();
  return AsyncMatch::MATCH;
}

bool TestSafeBrowsingDatabaseManager::MatchCsdWhitelistUrl(const GURL& url) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::MatchDownloadWhitelistString(
    const std::string& str) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::MatchDownloadWhitelistUrl(
    const GURL& url) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::MatchMalwareIP(
    const std::string& ip_address) {
  NOTIMPLEMENTED();
  return true;
}

safe_browsing::ThreatSource TestSafeBrowsingDatabaseManager::GetThreatSource()
    const {
  NOTIMPLEMENTED();
  return safe_browsing::ThreatSource::UNKNOWN;
}

bool TestSafeBrowsingDatabaseManager::IsCsdWhitelistKillSwitchOn() {
  NOTIMPLEMENTED();
  return false;
}

bool TestSafeBrowsingDatabaseManager::IsDownloadProtectionEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

bool TestSafeBrowsingDatabaseManager::IsMalwareKillSwitchOn() {
  NOTIMPLEMENTED();
  return false;
}

bool TestSafeBrowsingDatabaseManager::IsSupported() const {
  NOTIMPLEMENTED();
  return false;
}

void TestSafeBrowsingDatabaseManager::StartOnIOThread(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  SafeBrowsingDatabaseManager::StartOnIOThread(request_context_getter, config);
  enabled_ = true;
}

void TestSafeBrowsingDatabaseManager::StopOnIOThread(bool shutdown) {
  enabled_ = false;
  SafeBrowsingDatabaseManager::StopOnIOThread(shutdown);
}

}  // namespace safe_browsing
