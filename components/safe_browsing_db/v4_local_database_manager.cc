// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_local_database_manager.h"

#include <vector>

#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

V4LocalDatabaseManager::V4LocalDatabaseManager() : enabled_(false) {}

V4LocalDatabaseManager::~V4LocalDatabaseManager() {
  DCHECK(!enabled_);
}

bool V4LocalDatabaseManager::IsSupported() const {
  return true;
}

safe_browsing::ThreatSource V4LocalDatabaseManager::GetThreatSource() const {
  return safe_browsing::ThreatSource::LOCAL_PVER4;
}

bool V4LocalDatabaseManager::ChecksAreAlwaysAsync() const {
  return false;
}

bool V4LocalDatabaseManager::CanCheckResourceType(
    content::ResourceType resource_type) const {
  // We check all types since most checks are fast.
  return true;
}

bool V4LocalDatabaseManager::CanCheckUrl(const GURL& url) const {
  return url.SchemeIs(url::kHttpsScheme) || url.SchemeIs(url::kHttpScheme) ||
         url.SchemeIs(url::kFtpScheme);
}

bool V4LocalDatabaseManager::IsDownloadProtectionEnabled() const {
  // TODO(vakh): Investigate the possibility of using a command line switch for
  // this instead.
  return true;
}

bool V4LocalDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(vakh): Implement this skeleton.
  return true;
}

bool V4LocalDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::MatchMalwareIP(const std::string& ip_address) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return false;
}

bool V4LocalDatabaseManager::MatchCsdWhitelistUrl(const GURL& url) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::MatchDownloadWhitelistUrl(const GURL& url) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::MatchDownloadWhitelistString(
    const std::string& str) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::MatchInclusionWhitelistUrl(const GURL& url) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::MatchModuleWhitelistString(
    const std::string& str) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::CheckResourceUrl(const GURL& url, Client* client) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::IsMalwareKillSwitchOn() {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::IsCsdWhitelistKillSwitchOn() {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::CheckBrowseUrl(const GURL& url, Client* client) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!enabled_)
    return true;

  // Don't defer the resource load.
  return true;
}

void V4LocalDatabaseManager::CancelCheck(Client* client) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(enabled_);
}

void V4LocalDatabaseManager::StartOnIOThread(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  SafeBrowsingDatabaseManager::StartOnIOThread(request_context_getter, config);

#if defined(OS_WIN) || defined (OS_LINUX) || defined (OS_MACOSX)
  // TODO(vakh): Remove this if/endif block when the V4Database is implemented.
  // Filed as http://crbug.com/608075
  UpdateListIdentifier update_list_identifier;
#if defined(OS_WIN)
  update_list_identifier.platform_type = WINDOWS_PLATFORM;
#elif defined (OS_LINUX)
  update_list_identifier.platform_type = LINUX_PLATFORM;
#else
  update_list_identifier.platform_type = OSX_PLATFORM;
#endif
  update_list_identifier.threat_entry_type = URL_EXPRESSION;
  update_list_identifier.threat_type = MALWARE_THREAT;
  current_list_states_[update_list_identifier] = "";
#endif

  V4UpdateCallback callback = base::Bind(
      &V4LocalDatabaseManager::UpdateRequestCompleted, base::Unretained(this));
  v4_update_protocol_manager_ = V4UpdateProtocolManager::Create(
      request_context_getter, config, current_list_states_, callback);

  enabled_ = true;
}

void V4LocalDatabaseManager::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Delete the V4UpdateProtocolManager.
  // This cancels any in-flight update request.
  if (v4_update_protocol_manager_.get()) {
    v4_update_protocol_manager_.reset();
  }

  enabled_ = false;
  SafeBrowsingDatabaseManager::StopOnIOThread(shutdown);
}

void V4LocalDatabaseManager::UpdateRequestCompleted(
    const std::vector<ListUpdateResponse>& responses) {
  // TODO(vakh): Updates downloaded. Store them on disk and record new state.
}

}  // namespace safe_browsing
