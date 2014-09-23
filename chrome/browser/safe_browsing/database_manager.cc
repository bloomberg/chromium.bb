// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/database_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/leak_tracker.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/metrics_service.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "url/url_constants.h"

using content::BrowserThread;

namespace {

// Timeout for match checks, e.g. download URLs, hashes.
const int kCheckTimeoutMs = 10000;

// Records disposition information about the check.  |hit| should be
// |true| if there were any prefix hits in |full_hashes|.
void RecordGetHashCheckStatus(
    bool hit,
    safe_browsing_util::ListType check_type,
    const std::vector<SBFullHashResult>& full_hashes) {
  SafeBrowsingProtocolManager::ResultType result;
  if (full_hashes.empty()) {
    result = SafeBrowsingProtocolManager::GET_HASH_FULL_HASH_EMPTY;
  } else if (hit) {
    result = SafeBrowsingProtocolManager::GET_HASH_FULL_HASH_HIT;
  } else {
    result = SafeBrowsingProtocolManager::GET_HASH_FULL_HASH_MISS;
  }
  bool is_download = check_type == safe_browsing_util::BINURL;
  SafeBrowsingProtocolManager::RecordGetHashResult(is_download, result);
}

bool IsExpectedThreat(
    const SBThreatType threat_type,
    const std::vector<SBThreatType>& expected_threats) {
  return expected_threats.end() != std::find(expected_threats.begin(),
                                             expected_threats.end(),
                                             threat_type);
}

// Return the list id from the first result in |full_hashes| which matches
// |hash|, or INVALID if none match.
safe_browsing_util::ListType GetHashThreatListType(
    const SBFullHash& hash,
    const std::vector<SBFullHashResult>& full_hashes,
    size_t* index) {
  for (size_t i = 0; i < full_hashes.size(); ++i) {
    if (SBFullHashEqual(hash, full_hashes[i].hash)) {
      if (index)
        *index = i;
      return static_cast<safe_browsing_util::ListType>(full_hashes[i].list_id);
    }
  }
  return safe_browsing_util::INVALID;
}

// Given a URL, compare all the possible host + path full hashes to the set of
// provided full hashes.  Returns the list id of the a matching result from
// |full_hashes|, or INVALID if none match.
safe_browsing_util::ListType GetUrlThreatListType(
    const GURL& url,
    const std::vector<SBFullHashResult>& full_hashes,
    size_t* index) {
  if (full_hashes.empty())
    return safe_browsing_util::INVALID;

  std::vector<std::string> patterns;
  safe_browsing_util::GeneratePatternsToCheck(url, &patterns);

  for (size_t i = 0; i < patterns.size(); ++i) {
    safe_browsing_util::ListType threat = GetHashThreatListType(
        SBFullHashForString(patterns[i]), full_hashes, index);
    if (threat != safe_browsing_util::INVALID)
      return threat;
  }
  return safe_browsing_util::INVALID;
}

SBThreatType GetThreatTypeFromListType(safe_browsing_util::ListType list_type) {
  switch (list_type) {
    case safe_browsing_util::PHISH:
      return SB_THREAT_TYPE_URL_PHISHING;
    case safe_browsing_util::MALWARE:
      return SB_THREAT_TYPE_URL_MALWARE;
    case safe_browsing_util::BINURL:
      return SB_THREAT_TYPE_BINARY_MALWARE_URL;
    case safe_browsing_util::EXTENSIONBLACKLIST:
      return SB_THREAT_TYPE_EXTENSION;
    default:
      DVLOG(1) << "Unknown safe browsing list id " << list_type;
      return SB_THREAT_TYPE_SAFE;
  }
}

}  // namespace

// static
SBThreatType SafeBrowsingDatabaseManager::GetHashThreatType(
    const SBFullHash& hash,
    const std::vector<SBFullHashResult>& full_hashes) {
  return GetThreatTypeFromListType(
      GetHashThreatListType(hash, full_hashes, NULL));
}

// static
SBThreatType SafeBrowsingDatabaseManager::GetUrlThreatType(
    const GURL& url,
    const std::vector<SBFullHashResult>& full_hashes,
    size_t* index) {
  return GetThreatTypeFromListType(
      GetUrlThreatListType(url, full_hashes, index));
}

SafeBrowsingDatabaseManager::SafeBrowsingCheck::SafeBrowsingCheck(
    const std::vector<GURL>& urls,
    const std::vector<SBFullHash>& full_hashes,
    Client* client,
    safe_browsing_util::ListType check_type,
    const std::vector<SBThreatType>& expected_threats)
    : urls(urls),
      url_results(urls.size(), SB_THREAT_TYPE_SAFE),
      url_metadata(urls.size()),
      full_hashes(full_hashes),
      full_hash_results(full_hashes.size(), SB_THREAT_TYPE_SAFE),
      client(client),
      need_get_hash(false),
      check_type(check_type),
      expected_threats(expected_threats) {
  DCHECK_EQ(urls.empty(), !full_hashes.empty())
      << "Exactly one of urls and full_hashes must be set";
}

SafeBrowsingDatabaseManager::SafeBrowsingCheck::~SafeBrowsingCheck() {}

void SafeBrowsingDatabaseManager::Client::OnSafeBrowsingResult(
    const SafeBrowsingCheck& check) {
  DCHECK_EQ(check.urls.size(), check.url_results.size());
  DCHECK_EQ(check.full_hashes.size(), check.full_hash_results.size());
  if (!check.urls.empty()) {
    DCHECK(check.full_hashes.empty());
    switch (check.check_type) {
      case safe_browsing_util::MALWARE:
      case safe_browsing_util::PHISH:
        DCHECK_EQ(1u, check.urls.size());
        OnCheckBrowseUrlResult(
            check.urls[0], check.url_results[0], check.url_metadata[0]);
        break;
      case safe_browsing_util::BINURL:
        DCHECK_EQ(check.urls.size(), check.url_results.size());
        OnCheckDownloadUrlResult(
            check.urls,
            *std::max_element(check.url_results.begin(),
                              check.url_results.end()));
        break;
      default:
        NOTREACHED();
    }
  } else if (!check.full_hashes.empty()) {
    switch (check.check_type) {
      case safe_browsing_util::EXTENSIONBLACKLIST: {
        std::set<std::string> unsafe_extension_ids;
        for (size_t i = 0; i < check.full_hashes.size(); ++i) {
          std::string extension_id =
              safe_browsing_util::SBFullHashToString(check.full_hashes[i]);
          if (check.full_hash_results[i] == SB_THREAT_TYPE_EXTENSION)
            unsafe_extension_ids.insert(extension_id);
        }
        OnCheckExtensionsResult(unsafe_extension_ids);
        break;
      }
      default:
        NOTREACHED();
    }
  } else {
    NOTREACHED();
  }
}

SafeBrowsingDatabaseManager::SafeBrowsingDatabaseManager(
    const scoped_refptr<SafeBrowsingService>& service)
    : sb_service_(service),
      database_(NULL),
      enabled_(false),
      enable_download_protection_(false),
      enable_csd_whitelist_(false),
      enable_download_whitelist_(false),
      enable_extension_blacklist_(false),
      enable_side_effect_free_whitelist_(false),
      enable_ip_blacklist_(false),
      update_in_progress_(false),
      database_update_in_progress_(false),
      closing_database_(false),
      check_timeout_(base::TimeDelta::FromMilliseconds(kCheckTimeoutMs)) {
  DCHECK(sb_service_.get() != NULL);

  // Android only supports a subset of FULL_SAFE_BROWSING.
  // TODO(shess): This shouldn't be OS-driven <http://crbug.com/394379>
#if !defined(OS_ANDROID)
  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  enable_download_protection_ =
      !cmdline->HasSwitch(switches::kSbDisableDownloadProtection);

  // We only download the csd-whitelist if client-side phishing detection is
  // enabled.
  enable_csd_whitelist_ =
      !cmdline->HasSwitch(switches::kDisableClientSidePhishingDetection);

  // TODO(noelutz): remove this boolean variable since it should always be true
  // if SafeBrowsing is enabled.  Unfortunately, we have no test data for this
  // list right now.  This means that we need to be able to disable this list
  // for the SafeBrowsing test to pass.
  enable_download_whitelist_ = enable_csd_whitelist_;

  // TODO(kalman): there really shouldn't be a flag for this.
  enable_extension_blacklist_ =
      !cmdline->HasSwitch(switches::kSbDisableExtensionBlacklist);

  enable_side_effect_free_whitelist_ =
      prerender::IsSideEffectFreeWhitelistEnabled() &&
      !cmdline->HasSwitch(switches::kSbDisableSideEffectFreeWhitelist);

  // The client-side IP blacklist feature is tightly integrated with client-side
  // phishing protection for now.
  enable_ip_blacklist_ = enable_csd_whitelist_;

  enum SideEffectFreeWhitelistStatus {
    SIDE_EFFECT_FREE_WHITELIST_ENABLED,
    SIDE_EFFECT_FREE_WHITELIST_DISABLED,
    SIDE_EFFECT_FREE_WHITELIST_STATUS_MAX
  };

  SideEffectFreeWhitelistStatus side_effect_free_whitelist_status =
      enable_side_effect_free_whitelist_ ? SIDE_EFFECT_FREE_WHITELIST_ENABLED :
      SIDE_EFFECT_FREE_WHITELIST_DISABLED;

  UMA_HISTOGRAM_ENUMERATION("SB2.SideEffectFreeWhitelistStatus",
                            side_effect_free_whitelist_status,
                            SIDE_EFFECT_FREE_WHITELIST_STATUS_MAX);
#endif
}

SafeBrowsingDatabaseManager::~SafeBrowsingDatabaseManager() {
  // We should have already been shut down. If we're still enabled, then the
  // database isn't going to be closed properly, which could lead to corruption.
  DCHECK(!enabled_);
}

bool SafeBrowsingDatabaseManager::CanCheckUrl(const GURL& url) const {
  return url.SchemeIs(url::kFtpScheme) ||
         url.SchemeIs(url::kHttpScheme) ||
         url.SchemeIs(url::kHttpsScheme);
}

bool SafeBrowsingDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_ || !enable_download_protection_)
    return true;

  // We need to check the database for url prefix, and later may fetch the url
  // from the safebrowsing backends. These need to be asynchronous.
  SafeBrowsingCheck* check =
      new SafeBrowsingCheck(url_chain,
                            std::vector<SBFullHash>(),
                            client,
                            safe_browsing_util::BINURL,
                            std::vector<SBThreatType>(1,
                                SB_THREAT_TYPE_BINARY_MALWARE_URL));
  StartSafeBrowsingCheck(
      check,
      base::Bind(&SafeBrowsingDatabaseManager::CheckDownloadUrlOnSBThread, this,
                 check));
  return false;
}

bool SafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!enabled_ || !enable_extension_blacklist_)
    return true;

  std::vector<SBFullHash> extension_id_hashes;
  std::transform(extension_ids.begin(), extension_ids.end(),
                 std::back_inserter(extension_id_hashes),
                 safe_browsing_util::StringToSBFullHash);

  SafeBrowsingCheck* check = new SafeBrowsingCheck(
      std::vector<GURL>(),
      extension_id_hashes,
      client,
      safe_browsing_util::EXTENSIONBLACKLIST,
      std::vector<SBThreatType>(1, SB_THREAT_TYPE_EXTENSION));

  StartSafeBrowsingCheck(
      check,
      base::Bind(&SafeBrowsingDatabaseManager::CheckExtensionIDsOnSBThread,
                 this,
                 check));
  return false;
}

bool SafeBrowsingDatabaseManager::CheckSideEffectFreeWhitelistUrl(
    const GURL& url) {
  if (!enabled_)
    return false;

  if (!CanCheckUrl(url))
    return false;

  return database_->ContainsSideEffectFreeWhitelistUrl(url);
}

bool SafeBrowsingDatabaseManager::MatchMalwareIP(
    const std::string& ip_address) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_ || !enable_ip_blacklist_ || !MakeDatabaseAvailable()) {
    return false;  // Fail open.
  }
  return database_->ContainsMalwareIP(ip_address);
}

bool SafeBrowsingDatabaseManager::MatchCsdWhitelistUrl(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_ || !enable_csd_whitelist_ || !MakeDatabaseAvailable()) {
    // There is something funky going on here -- for example, perhaps the user
    // has not restarted since enabling metrics reporting, so we haven't
    // enabled the csd whitelist yet.  Just to be safe we return true in this
    // case.
    return true;
  }
  return database_->ContainsCsdWhitelistedUrl(url);
}

bool SafeBrowsingDatabaseManager::MatchDownloadWhitelistUrl(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_ || !enable_download_whitelist_ || !MakeDatabaseAvailable()) {
    return true;
  }
  return database_->ContainsDownloadWhitelistedUrl(url);
}

bool SafeBrowsingDatabaseManager::MatchDownloadWhitelistString(
    const std::string& str) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_ || !enable_download_whitelist_ || !MakeDatabaseAvailable()) {
    return true;
  }
  return database_->ContainsDownloadWhitelistedString(str);
}

bool SafeBrowsingDatabaseManager::IsMalwareKillSwitchOn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_ || !MakeDatabaseAvailable()) {
    return true;
  }
  return database_->IsMalwareIPMatchKillSwitchOn();
}

bool SafeBrowsingDatabaseManager::IsCsdWhitelistKillSwitchOn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_ || !MakeDatabaseAvailable()) {
    return true;
  }
  return database_->IsCsdWhitelistKillSwitchOn();
}

bool SafeBrowsingDatabaseManager::CheckBrowseUrl(const GURL& url,
                                                 Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_)
    return true;

  if (!CanCheckUrl(url))
    return true;

  std::vector<SBThreatType> expected_threats;
  expected_threats.push_back(SB_THREAT_TYPE_URL_MALWARE);
  expected_threats.push_back(SB_THREAT_TYPE_URL_PHISHING);

  const base::TimeTicks start = base::TimeTicks::Now();
  if (!MakeDatabaseAvailable()) {
    QueuedCheck queued_check(safe_browsing_util::MALWARE,  // or PHISH
                             client,
                             url,
                             expected_threats,
                             start);
    queued_checks_.push_back(queued_check);
    return false;
  }

  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> cache_hits;

  bool prefix_match =
      database_->ContainsBrowseUrl(url, &prefix_hits, &cache_hits);

  UMA_HISTOGRAM_TIMES("SB2.FilterCheck", base::TimeTicks::Now() - start);

  if (!prefix_match)
    return true;  // URL is okay.

  // Needs to be asynchronous, since we could be in the constructor of a
  // ResourceDispatcherHost event handler which can't pause there.
  SafeBrowsingCheck* check = new SafeBrowsingCheck(std::vector<GURL>(1, url),
                                                   std::vector<SBFullHash>(),
                                                   client,
                                                   safe_browsing_util::MALWARE,
                                                   expected_threats);
  check->need_get_hash = cache_hits.empty();
  check->prefix_hits.swap(prefix_hits);
  check->cache_hits.swap(cache_hits);
  checks_.insert(check);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::OnCheckDone, this, check));

  return false;
}

void SafeBrowsingDatabaseManager::CancelCheck(Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (CurrentChecks::iterator i = checks_.begin(); i != checks_.end(); ++i) {
    // We can't delete matching checks here because the db thread has a copy of
    // the pointer.  Instead, we simply NULL out the client, and when the db
    // thread calls us back, we'll clean up the check.
    if ((*i)->client == client)
      (*i)->client = NULL;
  }

  // Scan the queued clients store. Clients may be here if they requested a URL
  // check before the database has finished loading.
  for (std::deque<QueuedCheck>::iterator it(queued_checks_.begin());
       it != queued_checks_.end(); ) {
    // In this case it's safe to delete matches entirely since nothing has a
    // pointer to them.
    if (it->client == client)
      it = queued_checks_.erase(it);
    else
      ++it;
  }
}

void SafeBrowsingDatabaseManager::HandleGetHashResults(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes,
    const base::TimeDelta& cache_lifetime) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!enabled_)
    return;

  // If the service has been shut down, |check| should have been deleted.
  DCHECK(checks_.find(check) != checks_.end());

  // |start| is set before calling |GetFullHash()|, which should be
  // the only path which gets to here.
  DCHECK(!check->start.is_null());
  UMA_HISTOGRAM_LONG_TIMES("SB2.Network",
                           base::TimeTicks::Now() - check->start);

  std::vector<SBPrefix> prefixes = check->prefix_hits;
  OnHandleGetHashResults(check, full_hashes);  // 'check' is deleted here.

  // Cache the GetHash results.
  if (cache_lifetime != base::TimeDelta() && MakeDatabaseAvailable())
    database_->CacheHashResults(prefixes, full_hashes, cache_lifetime);
}

void SafeBrowsingDatabaseManager::GetChunks(GetChunksCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  DCHECK(!callback.is_null());
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, base::Bind(
      &SafeBrowsingDatabaseManager::GetAllChunksFromDatabase, this, callback));
}

void SafeBrowsingDatabaseManager::AddChunks(
    const std::string& list,
    scoped_ptr<ScopedVector<SBChunkData> > chunks,
    AddChunksCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  DCHECK(!callback.is_null());
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, base::Bind(
      &SafeBrowsingDatabaseManager::AddDatabaseChunks, this, list,
      base::Passed(&chunks), callback));
}

void SafeBrowsingDatabaseManager::DeleteChunks(
    scoped_ptr<std::vector<SBChunkDelete> > chunk_deletes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, base::Bind(
      &SafeBrowsingDatabaseManager::DeleteDatabaseChunks, this,
      base::Passed(&chunk_deletes)));
}

void SafeBrowsingDatabaseManager::UpdateStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  DCHECK(!update_in_progress_);
  update_in_progress_ = true;
}

void SafeBrowsingDatabaseManager::UpdateFinished(bool update_succeeded) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  if (update_in_progress_) {
    update_in_progress_ = false;
    safe_browsing_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::DatabaseUpdateFinished,
                 this, update_succeeded));
  }
}

void SafeBrowsingDatabaseManager::ResetDatabase() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, base::Bind(
      &SafeBrowsingDatabaseManager::OnResetDatabase, this));
}

void SafeBrowsingDatabaseManager::LogPauseDelay(base::TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SB2.Delay", time);
}

void SafeBrowsingDatabaseManager::StartOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (enabled_)
    return;

  DCHECK(!safe_browsing_thread_.get());
  safe_browsing_thread_.reset(new base::Thread("Chrome_SafeBrowsingThread"));
  if (!safe_browsing_thread_->Start())
    return;
  enabled_ = true;

  MakeDatabaseAvailable();
}

void SafeBrowsingDatabaseManager::StopOnIOThread(bool shutdown) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DoStopOnIOThread();
  if (shutdown) {
    sb_service_ = NULL;
  }
}

void SafeBrowsingDatabaseManager::NotifyDatabaseUpdateFinished(
    bool update_succeeded) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SAFE_BROWSING_UPDATE_COMPLETE,
      content::Source<SafeBrowsingDatabaseManager>(this),
      content::Details<bool>(&update_succeeded));
}

SafeBrowsingDatabaseManager::QueuedCheck::QueuedCheck(
    const safe_browsing_util::ListType check_type,
    Client* client,
    const GURL& url,
    const std::vector<SBThreatType>& expected_threats,
    const base::TimeTicks& start)
    : check_type(check_type),
      client(client),
      url(url),
      expected_threats(expected_threats),
      start(start) {
}

SafeBrowsingDatabaseManager::QueuedCheck::~QueuedCheck() {
}

void SafeBrowsingDatabaseManager::DoStopOnIOThread() {
  if (!enabled_)
    return;

  enabled_ = false;

  // Delete queued checks, calling back any clients with 'SB_THREAT_TYPE_SAFE'.
  while (!queued_checks_.empty()) {
    QueuedCheck queued = queued_checks_.front();
    if (queued.client) {
      SafeBrowsingCheck sb_check(std::vector<GURL>(1, queued.url),
                                 std::vector<SBFullHash>(),
                                 queued.client,
                                 queued.check_type,
                                 queued.expected_threats);
      queued.client->OnSafeBrowsingResult(sb_check);
    }
    queued_checks_.pop_front();
  }

  // Close the database.  Cases to avoid:
  //  * If |closing_database_| is true, continuing will queue up a second
  //    request, |closing_database_| will be reset after handling the first
  //    request, and if any functions on the db thread recreate the database, we
  //    could start using it on the IO thread and then have the second request
  //    handler delete it out from under us.
  //  * If |database_| is NULL, then either no creation request is in flight, in
  //    which case we don't need to do anything, or one is in flight, in which
  //    case the database will be recreated before our deletion request is
  //    handled, and could be used on the IO thread in that time period, leading
  //    to the same problem as above.
  // Checking DatabaseAvailable() avoids both of these.
  if (DatabaseAvailable()) {
    closing_database_ = true;
    safe_browsing_thread_->message_loop()->PostTask(FROM_HERE,
        base::Bind(&SafeBrowsingDatabaseManager::OnCloseDatabase, this));
  }

  // Flush the database thread. Any in-progress database check results will be
  // ignored and cleaned up below.
  //
  // Note that to avoid leaking the database, we rely on the fact that no new
  // tasks will be added to the db thread between the call above and this one.
  // See comments on the declaration of |safe_browsing_thread_|.
  {
    // A ScopedAllowIO object is required to join the thread when calling Stop.
    // See http://crbug.com/72696. Note that we call Stop() first to clear out
    // any remaining tasks before clearing safe_browsing_thread_.
    base::ThreadRestrictions::ScopedAllowIO allow_io_for_thread_join;
    safe_browsing_thread_->Stop();
    safe_browsing_thread_.reset();
  }

  // Delete pending checks, calling back any clients with 'SB_THREAT_TYPE_SAFE'.
  // We have to do this after the db thread returns because methods on it can
  // have copies of these pointers, so deleting them might lead to accessing
  // garbage.
  for (CurrentChecks::iterator it = checks_.begin();
       it != checks_.end(); ++it) {
    SafeBrowsingCheck* check = *it;
    if (check->client)
      check->client->OnSafeBrowsingResult(*check);
  }
  STLDeleteElements(&checks_);

  gethash_requests_.clear();
}

bool SafeBrowsingDatabaseManager::DatabaseAvailable() const {
  base::AutoLock lock(database_lock_);
  return !closing_database_ && (database_ != NULL);
}

bool SafeBrowsingDatabaseManager::MakeDatabaseAvailable() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  if (DatabaseAvailable())
    return true;
  safe_browsing_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&SafeBrowsingDatabaseManager::GetDatabase),
                 this));
  return false;
}

SafeBrowsingDatabase* SafeBrowsingDatabaseManager::GetDatabase() {
  DCHECK_EQ(base::MessageLoop::current(),
            safe_browsing_thread_->message_loop());
  if (database_)
    return database_;
  startup_metric_utils::ScopedSlowStartupUMA
      scoped_timer("Startup.SlowStartupSafeBrowsingGetDatabase");
  const base::TimeTicks before = base::TimeTicks::Now();

  SafeBrowsingDatabase* database =
      SafeBrowsingDatabase::Create(enable_download_protection_,
                                   enable_csd_whitelist_,
                                   enable_download_whitelist_,
                                   enable_extension_blacklist_,
                                   enable_side_effect_free_whitelist_,
                                   enable_ip_blacklist_);

  database->Init(SafeBrowsingService::GetBaseFilename());
  {
    // Acquiring the lock here guarantees correct ordering between the writes to
    // the new database object above, and the setting of |databse_| below.
    base::AutoLock lock(database_lock_);
    database_ = database;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::DatabaseLoadComplete, this));

  UMA_HISTOGRAM_TIMES("SB2.DatabaseOpen", base::TimeTicks::Now() - before);
  return database_;
}

void SafeBrowsingDatabaseManager::OnCheckDone(SafeBrowsingCheck* check) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!enabled_)
    return;

  // If the service has been shut down, |check| should have been deleted.
  DCHECK(checks_.find(check) != checks_.end());

  if (check->client && check->need_get_hash) {
    // We have a partial match so we need to query Google for the full hash.
    // Clean up will happen in HandleGetHashResults.

    // See if we have a GetHash request already in progress for this particular
    // prefix. If so, we just append ourselves to the list of interested parties
    // when the results arrive. We only do this for checks involving one prefix,
    // since that is the common case (multiple prefixes will issue the request
    // as normal).
    if (check->prefix_hits.size() == 1) {
      SBPrefix prefix = check->prefix_hits[0];
      GetHashRequests::iterator it = gethash_requests_.find(prefix);
      if (it != gethash_requests_.end()) {
        // There's already a request in progress.
        it->second.push_back(check);
        return;
      }

      // No request in progress, so we're the first for this prefix.
      GetHashRequestors requestors;
      requestors.push_back(check);
      gethash_requests_[prefix] = requestors;
    }

    // Reset the start time so that we can measure the network time without the
    // database time.
    check->start = base::TimeTicks::Now();
    // Note: If |this| is deleted or stopped, the protocol_manager will
    // be destroyed as well - hence it's OK to do unretained in this case.
    bool is_download = check->check_type == safe_browsing_util::BINURL;
    sb_service_->protocol_manager()->GetFullHash(
        check->prefix_hits,
        base::Bind(&SafeBrowsingDatabaseManager::HandleGetHashResults,
                   base::Unretained(this),
                   check),
        is_download);
  } else {
    // We may have cached results for previous GetHash queries.  Since
    // this data comes from cache, don't histogram hits.
    bool is_threat = HandleOneCheck(check, check->cache_hits);
    // cache_hits should only contain hits for a fullhash we searched for, so if
    // we got to this point it should always result in a threat match.
    DCHECK(is_threat);
  }
}

void SafeBrowsingDatabaseManager::GetAllChunksFromDatabase(
    GetChunksCallback callback) {
  DCHECK_EQ(base::MessageLoop::current(),
            safe_browsing_thread_->message_loop());

  bool database_error = true;
  std::vector<SBListChunkRanges> lists;
  DCHECK(!database_update_in_progress_);
  database_update_in_progress_ = true;
  GetDatabase();  // This guarantees that |database_| is non-NULL.
  if (database_->UpdateStarted(&lists)) {
    database_error = false;
  } else {
    database_->UpdateFinished(false);
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::OnGetAllChunksFromDatabase,
                 this, lists, database_error, callback));
}

void SafeBrowsingDatabaseManager::OnGetAllChunksFromDatabase(
    const std::vector<SBListChunkRanges>& lists, bool database_error,
    GetChunksCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (enabled_)
    callback.Run(lists, database_error);
}

void SafeBrowsingDatabaseManager::OnAddChunksComplete(
    AddChunksCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (enabled_)
    callback.Run();
}

void SafeBrowsingDatabaseManager::DatabaseLoadComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_)
    return;

  LOCAL_HISTOGRAM_COUNTS("SB.QueueDepth", queued_checks_.size());
  if (queued_checks_.empty())
    return;

  // If the database isn't already available, calling CheckUrl() in the loop
  // below will add the check back to the queue, and we'll infinite-loop.
  DCHECK(DatabaseAvailable());
  while (!queued_checks_.empty()) {
    QueuedCheck check = queued_checks_.front();
    DCHECK(!check.start.is_null());
    LOCAL_HISTOGRAM_TIMES("SB.QueueDelay",
                          base::TimeTicks::Now() - check.start);
    // If CheckUrl() determines the URL is safe immediately, it doesn't call the
    // client's handler function (because normally it's being directly called by
    // the client).  Since we're not the client, we have to convey this result.
    if (check.client && CheckBrowseUrl(check.url, check.client)) {
      SafeBrowsingCheck sb_check(std::vector<GURL>(1, check.url),
                                 std::vector<SBFullHash>(),
                                 check.client,
                                 check.check_type,
                                 check.expected_threats);
      check.client->OnSafeBrowsingResult(sb_check);
    }
    queued_checks_.pop_front();
  }
}

void SafeBrowsingDatabaseManager::AddDatabaseChunks(
    const std::string& list_name,
    scoped_ptr<ScopedVector<SBChunkData> > chunks,
    AddChunksCallback callback) {
  DCHECK_EQ(base::MessageLoop::current(),
            safe_browsing_thread_->message_loop());
  if (chunks)
    GetDatabase()->InsertChunks(list_name, chunks->get());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::OnAddChunksComplete, this,
                 callback));
}

void SafeBrowsingDatabaseManager::DeleteDatabaseChunks(
    scoped_ptr<std::vector<SBChunkDelete> > chunk_deletes) {
  DCHECK_EQ(base::MessageLoop::current(),
            safe_browsing_thread_->message_loop());
  if (chunk_deletes)
    GetDatabase()->DeleteChunks(*chunk_deletes);
}

void SafeBrowsingDatabaseManager::DatabaseUpdateFinished(
    bool update_succeeded) {
  DCHECK_EQ(base::MessageLoop::current(),
            safe_browsing_thread_->message_loop());
  GetDatabase()->UpdateFinished(update_succeeded);
  DCHECK(database_update_in_progress_);
  database_update_in_progress_ = false;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::NotifyDatabaseUpdateFinished,
                 this, update_succeeded));
}

void SafeBrowsingDatabaseManager::OnCloseDatabase() {
  DCHECK_EQ(base::MessageLoop::current(),
            safe_browsing_thread_->message_loop());
  DCHECK(closing_database_);

  // Because |closing_database_| is true, nothing on the IO thread will be
  // accessing the database, so it's safe to delete and then NULL the pointer.
  delete database_;
  database_ = NULL;

  // Acquiring the lock here guarantees correct ordering between the resetting
  // of |database_| above and of |closing_database_| below, which ensures there
  // won't be a window during which the IO thread falsely believes the database
  // is available.
  base::AutoLock lock(database_lock_);
  closing_database_ = false;
}

void SafeBrowsingDatabaseManager::OnResetDatabase() {
  DCHECK_EQ(base::MessageLoop::current(),
            safe_browsing_thread_->message_loop());
  GetDatabase()->ResetDatabase();
}

void SafeBrowsingDatabaseManager::OnHandleGetHashResults(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  safe_browsing_util::ListType check_type = check->check_type;
  SBPrefix prefix = check->prefix_hits[0];
  GetHashRequests::iterator it = gethash_requests_.find(prefix);
  if (check->prefix_hits.size() > 1 || it == gethash_requests_.end()) {
    const bool hit = HandleOneCheck(check, full_hashes);
    RecordGetHashCheckStatus(hit, check_type, full_hashes);
    return;
  }

  // Call back all interested parties, noting if any has a hit.
  GetHashRequestors& requestors = it->second;
  bool hit = false;
  for (GetHashRequestors::iterator r = requestors.begin();
       r != requestors.end(); ++r) {
    if (HandleOneCheck(*r, full_hashes))
      hit = true;
  }
  RecordGetHashCheckStatus(hit, check_type, full_hashes);

  gethash_requests_.erase(it);
}

bool SafeBrowsingDatabaseManager::HandleOneCheck(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(check);

  bool is_threat = false;

  // TODO(shess): GetHashThreadListType() contains a loop,
  // GetUrlThreatListType() a loop around that loop.  Having another loop out
  // here concerns me.  It is likely that SAFE is an expected outcome, which
  // means all of those loops run to completion.  Refactoring this to generate a
  // set of sorted items to compare in sequence would probably improve things.
  //
  // Additionally, the set of patterns generated from the urls is very similar
  // to the patterns generated in ContainsBrowseUrl() and other database checks,
  // which are called from this code.  Refactoring that across the checks could
  // interact well with batching the checks here.

  for (size_t i = 0; i < check->urls.size(); ++i) {
    size_t threat_index;
    SBThreatType threat =
        GetUrlThreatType(check->urls[i], full_hashes, &threat_index);
    if (threat != SB_THREAT_TYPE_SAFE &&
        IsExpectedThreat(threat, check->expected_threats)) {
      check->url_results[i] = threat;
      check->url_metadata[i] = full_hashes[threat_index].metadata;
      is_threat = true;
    }
  }

  for (size_t i = 0; i < check->full_hashes.size(); ++i) {
    SBThreatType threat = GetHashThreatType(check->full_hashes[i], full_hashes);
    if (threat != SB_THREAT_TYPE_SAFE &&
        IsExpectedThreat(threat, check->expected_threats)) {
      check->full_hash_results[i] = threat;
      is_threat = true;
    }
  }

  SafeBrowsingCheckDone(check);
  return is_threat;
}

void SafeBrowsingDatabaseManager::CheckDownloadUrlOnSBThread(
    SafeBrowsingCheck* check) {
  DCHECK_EQ(base::MessageLoop::current(),
            safe_browsing_thread_->message_loop());
  DCHECK(enable_download_protection_);

  std::vector<SBPrefix> prefix_hits;

  if (!database_->ContainsDownloadUrl(check->urls, &prefix_hits)) {
    // Good, we don't have hash for this url prefix.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingDatabaseManager::CheckDownloadUrlDone, this,
                   check));
    return;
  }

  check->need_get_hash = true;
  check->prefix_hits.clear();
  check->prefix_hits = prefix_hits;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::OnCheckDone, this, check));
}

void SafeBrowsingDatabaseManager::CheckExtensionIDsOnSBThread(
    SafeBrowsingCheck* check) {
  DCHECK_EQ(base::MessageLoop::current(),
            safe_browsing_thread_->message_loop());

  std::vector<SBPrefix> prefixes;
  for (std::vector<SBFullHash>::iterator it = check->full_hashes.begin();
       it != check->full_hashes.end(); ++it) {
    prefixes.push_back((*it).prefix);
  }
  database_->ContainsExtensionPrefixes(prefixes, &check->prefix_hits);

  if (check->prefix_hits.empty()) {
    // No matches for any extensions.
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SafeBrowsingDatabaseManager::SafeBrowsingCheckDone, this,
                   check));
  } else {
    // Some prefixes matched, we need to ask Google whether they're legit.
    check->need_get_hash = true;
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SafeBrowsingDatabaseManager::OnCheckDone, this, check));
  }
}

void SafeBrowsingDatabaseManager::TimeoutCallback(SafeBrowsingCheck* check) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(check);

  if (!enabled_)
    return;

  DCHECK(checks_.find(check) != checks_.end());
  if (check->client) {
    check->client->OnSafeBrowsingResult(*check);
    check->client = NULL;
  }
}

void SafeBrowsingDatabaseManager::CheckDownloadUrlDone(
    SafeBrowsingCheck* check) {
  DCHECK(enable_download_protection_);
  SafeBrowsingCheckDone(check);
}

void SafeBrowsingDatabaseManager::SafeBrowsingCheckDone(
    SafeBrowsingCheck* check) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(check);

  if (!enabled_)
    return;

  VLOG(1) << "SafeBrowsingCheckDone";
  DCHECK(checks_.find(check) != checks_.end());
  if (check->client)
    check->client->OnSafeBrowsingResult(*check);
  checks_.erase(check);
  delete check;
}

void SafeBrowsingDatabaseManager::StartSafeBrowsingCheck(
    SafeBrowsingCheck* check,
    const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  check->timeout_factory_.reset(
      new base::WeakPtrFactory<SafeBrowsingDatabaseManager>(this));
  checks_.insert(check);

  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, task);

  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::TimeoutCallback,
                 check->timeout_factory_->GetWeakPtr(), check),
      check_timeout_);
}
