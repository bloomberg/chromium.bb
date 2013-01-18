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
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/startup_metric_utils.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

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
  bool is_download = check_type == safe_browsing_util::BINURL ||
                     check_type == safe_browsing_util::BINHASH;
  SafeBrowsingProtocolManager::RecordGetHashResult(is_download, result);
}

}  // namespace

SafeBrowsingDatabaseManager::SafeBrowsingCheck::SafeBrowsingCheck(
    const std::vector<GURL>& urls,
    const std::vector<SBFullHash>& full_hashes,
    Client* client,
    safe_browsing_util::ListType check_type)
    : urls(urls),
      url_results(urls.size(), SB_THREAT_TYPE_SAFE),
      full_hashes(full_hashes),
      full_hash_results(full_hashes.size(), SB_THREAT_TYPE_SAFE),
      client(client),
      need_get_hash(false),
      check_type(check_type),
      timeout_factory_(NULL) {
  DCHECK(urls.empty() || full_hashes.empty()) <<
      "There must be either urls or full_hashes to check";
  DCHECK(!urls.empty() || !full_hashes.empty()) <<
      "There cannot be both urls and full_hashes to check";
}

SafeBrowsingDatabaseManager::SafeBrowsingCheck::~SafeBrowsingCheck() {}

void SafeBrowsingDatabaseManager::Client::OnSafeBrowsingResult(
    const SafeBrowsingCheck& check) {
  if (!check.urls.empty()) {
    DCHECK(check.full_hashes.empty());
    switch (check.check_type) {
      case safe_browsing_util::MALWARE:
      case safe_browsing_util::PHISH:
        DCHECK_EQ(1u, check.urls.size());
        DCHECK_EQ(1u, check.url_results.size());
        OnCheckBrowseUrlResult(check.urls[0], check.url_results[0]);
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
      case safe_browsing_util::BINHASH:
        DCHECK_EQ(1u, check.full_hashes.size());
        DCHECK_EQ(1u, check.full_hash_results.size());
        OnCheckDownloadHashResult(
            safe_browsing_util::SBFullHashToString(check.full_hashes[0]),
            check.full_hash_results[0]);
        break;
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
      update_in_progress_(false),
      database_update_in_progress_(false),
      closing_database_(false),
      check_timeout_(base::TimeDelta::FromMilliseconds(kCheckTimeoutMs)) {
  DCHECK(sb_service_ != NULL);

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
}

SafeBrowsingDatabaseManager::~SafeBrowsingDatabaseManager() {
  // We should have already been shut down. If we're still enabled, then the
  // database isn't going to be closed properly, which could lead to corruption.
  DCHECK(!enabled_);
}

bool SafeBrowsingDatabaseManager::CanCheckUrl(const GURL& url) const {
  return url.SchemeIs(chrome::kFtpScheme) ||
         url.SchemeIs(chrome::kHttpScheme) ||
         url.SchemeIs(chrome::kHttpsScheme);
}

bool SafeBrowsingDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_ || !enable_download_protection_)
    return true;

  // We need to check the database for url prefix, and later may fetch the url
  // from the safebrowsing backends. These need to be asynchronous.
  SafeBrowsingCheck* check = new SafeBrowsingCheck(url_chain,
                                                   std::vector<SBFullHash>(),
                                                   client,
                                                   safe_browsing_util::BINURL);
  StartSafeBrowsingCheck(
      check,
      base::Bind(&SafeBrowsingDatabaseManager::CheckDownloadUrlOnSBThread, this,
                 check));
  return false;
}

bool SafeBrowsingDatabaseManager::CheckDownloadHash(
    const std::string& full_hash,
    Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!full_hash.empty());
  if (!enabled_ || !enable_download_protection_ || full_hash.empty())
    return true;

  // We need to check the database for url prefix, and later may fetch the url
  // from the safebrowsing backends. These need to be asynchronous.
  std::vector<SBFullHash> full_hashes(
      1, safe_browsing_util::StringToSBFullHash(full_hash));
  SafeBrowsingCheck* check = new SafeBrowsingCheck(std::vector<GURL>(),
                                                   full_hashes,
                                                   client,
                                                   safe_browsing_util::BINHASH);
  StartSafeBrowsingCheck(
      check,
      base::Bind(&SafeBrowsingDatabaseManager::CheckDownloadHashOnSBThread,this,
                 check));
  return false;
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

bool SafeBrowsingDatabaseManager::CheckBrowseUrl(const GURL& url,
                                                 Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_)
    return true;

  if (!CanCheckUrl(url))
    return true;

  const base::TimeTicks start = base::TimeTicks::Now();
  if (!MakeDatabaseAvailable()) {
    QueuedCheck check;
    check.check_type = safe_browsing_util::MALWARE;  // or PHISH
    check.client = client;
    check.url = url;
    check.start = start;
    queued_checks_.push_back(check);
    return false;
  }

  std::string list;
  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> full_hits;

  bool prefix_match =
      database_->ContainsBrowseUrl(url, &list, &prefix_hits, &full_hits,
          sb_service_->protocol_manager()->last_update());

  UMA_HISTOGRAM_TIMES("SB2.FilterCheck", base::TimeTicks::Now() - start);

  if (!prefix_match)
    return true;  // URL is okay.

  // Needs to be asynchronous, since we could be in the constructor of a
  // ResourceDispatcherHost event handler which can't pause there.
  SafeBrowsingCheck* check = new SafeBrowsingCheck(std::vector<GURL>(1, url),
                                                   std::vector<SBFullHash>(),
                                                   client,
                                                   safe_browsing_util::MALWARE);
  check->need_get_hash = full_hits.empty();
  check->prefix_hits.swap(prefix_hits);
  check->full_hits.swap(full_hits);
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
    bool can_cache) {
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

  if (can_cache && MakeDatabaseAvailable()) {
    // Cache the GetHash results in memory:
    database_->CacheHashResults(prefixes, full_hashes);
  }
}

void SafeBrowsingDatabaseManager::GetChunks(GetChunksCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  DCHECK(!callback.is_null());
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, base::Bind(
      &SafeBrowsingDatabaseManager::GetAllChunksFromDatabase, this, callback));
}

void SafeBrowsingDatabaseManager::AddChunks(const std::string& list,
                                            SBChunkList* chunks,
                                            AddChunksCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  DCHECK(!callback.is_null());
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, base::Bind(
      &SafeBrowsingDatabaseManager::AddDatabaseChunks, this, list,
      chunks, callback));
}

void SafeBrowsingDatabaseManager::DeleteChunks(
    std::vector<SBChunkDelete>* chunk_deletes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, base::Bind(
      &SafeBrowsingDatabaseManager::DeleteDatabaseChunks, this, chunk_deletes));
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

void SafeBrowsingDatabaseManager::PurgeMemory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  CloseDatabase();
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

void SafeBrowsingDatabaseManager::DoStopOnIOThread() {
  if (!enabled_)
    return;

  enabled_ = false;

  // Delete queued checks, calling back any clients with 'SB_THREAT_TYPE_SAFE'.
  // If we don't do this here we may fail to close the database below.
  while (!queued_checks_.empty()) {
    QueuedCheck queued = queued_checks_.front();
    if (queued.client) {
      SafeBrowsingCheck sb_check(std::vector<GURL>(1, queued.url),
                                 std::vector<SBFullHash>(),
                                 queued.client,
                                 queued.check_type);
      queued.client->OnSafeBrowsingResult(sb_check);
    }
    queued_checks_.pop_front();
  }

  // Close the database.  We don't simply DeleteSoon() because if a close is
  // already pending, we'll double-free, and we don't set |database_| to NULL
  // because if there is still anything running on the db thread, it could
  // create a new database object (via GetDatabase()) that would then leak.
  CloseDatabase();

  // Flush the database thread. Any in-progress database check results will be
  // ignored and cleaned up below.
  //
  // Note that to avoid leaking the database, we rely on the fact that no new
  // tasks will be added to the db thread between the call above and this one.
  // See comments on the declaration of |safe_browsing_thread_|.
  {
    // A ScopedAllowIO object is required to join the thread when calling Stop.
    // See http://crbug.com/72696.
    base::ThreadRestrictions::ScopedAllowIO allow_io_for_thread_join;
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

void SafeBrowsingDatabaseManager::CloseDatabase() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Cases to avoid:
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
  //  * If |queued_checks_| is non-empty and |database_| is non-NULL, we're
  //    about to be called back (in DatabaseLoadComplete()).  This will call
  //    CheckUrl(), which will want the database.  Closing the database here
  //    would lead to an infinite loop in DatabaseLoadComplete(), and even if it
  //    didn't, it would be pointless since we'd just want to recreate.
  //
  // The first two cases above are handled by checking DatabaseAvailable().
  if (!DatabaseAvailable() || !queued_checks_.empty())
    return;

  closing_database_ = true;
  if (safe_browsing_thread_.get()) {
    safe_browsing_thread_->message_loop()->PostTask(FROM_HERE,
        base::Bind(&SafeBrowsingDatabaseManager::OnCloseDatabase, this));
  }
}

SafeBrowsingDatabase* SafeBrowsingDatabaseManager::GetDatabase() {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  if (database_)
    return database_;
  startup_metric_utils::ScopedSlowStartupUMA
      scoped_timer("Startup.SlowStartupSafeBrowsingGetDatabase");
  const base::TimeTicks before = base::TimeTicks::Now();

  SafeBrowsingDatabase* database =
      SafeBrowsingDatabase::Create(enable_download_protection_,
                                   enable_csd_whitelist_,
                                   enable_download_whitelist_);

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
    bool is_download = check->check_type == safe_browsing_util::BINURL ||
                       check->check_type == safe_browsing_util::BINHASH;
    sb_service_->protocol_manager()->GetFullHash(
        check->prefix_hits,
        base::Bind(&SafeBrowsingDatabaseManager::HandleGetHashResults,
                   base::Unretained(this),
                   check),
        is_download);
  } else {
    // We may have cached results for previous GetHash queries.  Since
    // this data comes from cache, don't histogram hits.
    HandleOneCheck(check, check->full_hits);
  }
}

void SafeBrowsingDatabaseManager::GetAllChunksFromDatabase(
    GetChunksCallback callback) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());

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

  HISTOGRAM_COUNTS("SB.QueueDepth", queued_checks_.size());
  if (queued_checks_.empty())
    return;

  // If the database isn't already available, calling CheckUrl() in the loop
  // below will add the check back to the queue, and we'll infinite-loop.
  DCHECK(DatabaseAvailable());
  while (!queued_checks_.empty()) {
    QueuedCheck check = queued_checks_.front();
    DCHECK(!check.start.is_null());
    HISTOGRAM_TIMES("SB.QueueDelay", base::TimeTicks::Now() - check.start);
    // If CheckUrl() determines the URL is safe immediately, it doesn't call the
    // client's handler function (because normally it's being directly called by
    // the client).  Since we're not the client, we have to convey this result.
    if (check.client && CheckBrowseUrl(check.url, check.client)) {
      SafeBrowsingCheck sb_check(std::vector<GURL>(1, check.url),
                                 std::vector<SBFullHash>(),
                                 check.client,
                                 check.check_type);
      check.client->OnSafeBrowsingResult(sb_check);
    }
    queued_checks_.pop_front();
  }
}

void SafeBrowsingDatabaseManager::AddDatabaseChunks(
    const std::string& list_name, SBChunkList* chunks,
    AddChunksCallback callback) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  if (chunks) {
    GetDatabase()->InsertChunks(list_name, *chunks);
    delete chunks;
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::OnAddChunksComplete, this,
                 callback));
}

void SafeBrowsingDatabaseManager::DeleteDatabaseChunks(
    std::vector<SBChunkDelete>* chunk_deletes) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  if (chunk_deletes) {
    GetDatabase()->DeleteChunks(*chunk_deletes);
    delete chunk_deletes;
  }
}

SBThreatType SafeBrowsingDatabaseManager::GetThreatTypeFromListname(
    const std::string& list_name) {
  if (safe_browsing_util::IsPhishingList(list_name)) {
    return SB_THREAT_TYPE_URL_PHISHING;
  }

  if (safe_browsing_util::IsMalwareList(list_name)) {
    return SB_THREAT_TYPE_URL_MALWARE;
  }

  if (safe_browsing_util::IsBadbinurlList(list_name)) {
    return SB_THREAT_TYPE_BINARY_MALWARE_URL;
  }

  if (safe_browsing_util::IsBadbinhashList(list_name)) {
    return SB_THREAT_TYPE_BINARY_MALWARE_HASH;
  }

  DVLOG(1) << "Unknown safe browsing list " << list_name;
  return SB_THREAT_TYPE_SAFE;
}

void SafeBrowsingDatabaseManager::DatabaseUpdateFinished(
    bool update_succeeded) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  GetDatabase()->UpdateFinished(update_succeeded);
  DCHECK(database_update_in_progress_);
  database_update_in_progress_ = false;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::NotifyDatabaseUpdateFinished,
                 this, update_succeeded));
}

void SafeBrowsingDatabaseManager::NotifyDatabaseUpdateFinished(
    bool update_succeeded) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SAFE_BROWSING_UPDATE_COMPLETE,
      content::Source<SafeBrowsingDatabaseManager>(this),
      content::Details<bool>(&update_succeeded));
}

void SafeBrowsingDatabaseManager::OnCloseDatabase() {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
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
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  GetDatabase()->ResetDatabase();
}

void SafeBrowsingDatabaseManager::CacheHashResults(
  const std::vector<SBPrefix>& prefixes,
  const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  GetDatabase()->CacheHashResults(prefixes, full_hashes);
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

  for (size_t i = 0; i < check->urls.size(); ++i) {
    int index =
        safe_browsing_util::GetUrlHashIndex(check->urls[i], full_hashes);
    if (index == -1)
      continue;
    SBThreatType threat =
        GetThreatTypeFromListname(full_hashes[index].list_name);
    if (threat != SB_THREAT_TYPE_SAFE) {
      check->url_results[i] = threat;
      is_threat = true;
    } else {
      NOTREACHED();
    }
  }

  for (size_t i = 0; i < check->full_hashes.size(); ++i) {
    int index =
        safe_browsing_util::GetHashIndex(check->full_hashes[i], full_hashes);
    if (index == -1)
      continue;
    SBThreatType threat =
        GetThreatTypeFromListname(full_hashes[index].list_name);
    if (threat != SB_THREAT_TYPE_SAFE) {
      check->full_hash_results[i] = threat;
      is_threat = true;
    } else {
      NOTREACHED();
    }
  }

  SafeBrowsingCheckDone(check);
  return is_threat;
}

void SafeBrowsingDatabaseManager::CheckDownloadHashOnSBThread(
    SafeBrowsingCheck* check) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  DCHECK(enable_download_protection_);

  DCHECK_EQ(1u, check->full_hashes.size());
  SBFullHash full_hash = check->full_hashes[0];

  if (!database_->ContainsDownloadHashPrefix(full_hash.prefix)) {
    // Good, we don't have hash for this url prefix.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingDatabaseManager::CheckDownloadHashDone, this,
                   check));
    return;
  }

  check->need_get_hash = true;
  check->prefix_hits.push_back(full_hash.prefix);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::OnCheckDone, this, check));
}

void SafeBrowsingDatabaseManager::CheckDownloadUrlOnSBThread(
    SafeBrowsingCheck* check) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
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

void SafeBrowsingDatabaseManager::CheckDownloadHashDone(
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

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::TimeoutCallback,
                 check->timeout_factory_->GetWeakPtr(), check),
      check_timeout_);
}
