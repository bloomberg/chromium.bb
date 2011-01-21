// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "net/base/registry_controlled_domain.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#endif

using base::Time;
using base::TimeDelta;

namespace {

// The default URL prefix where browser fetches chunk updates, hashes,
// and reports safe browsing hits.
const char* const kSbDefaultInfoURLPrefix =
    "http://safebrowsing.clients.google.com/safebrowsing";

// The default URL prefix where browser fetches MAC client key and reports
// malware details.
const char* const kSbDefaultMacKeyURLPrefix =
    "https://sb-ssl.google.com/safebrowsing";

// TODO(lzheng): Replace this with Profile* ProfileManager::GetDefaultProfile().
Profile* GetDefaultProfile() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetDefaultProfile(user_data_dir);
}

// Records disposition information about the check.  |hit| should be
// |true| if there were any prefix hits in |full_hashes|.
void RecordGetHashCheckStatus(
    bool hit, const std::vector<SBFullHashResult>& full_hashes) {
  SafeBrowsingProtocolManager::ResultType result;
  if (full_hashes.empty()) {
    result = SafeBrowsingProtocolManager::GET_HASH_FULL_HASH_EMPTY;
  } else if (hit) {
    result = SafeBrowsingProtocolManager::GET_HASH_FULL_HASH_HIT;
  } else {
    result = SafeBrowsingProtocolManager::GET_HASH_FULL_HASH_MISS;
  }
  SafeBrowsingProtocolManager::RecordGetHashResult(result);
}

}  // namespace

// static
SafeBrowsingServiceFactory* SafeBrowsingService::factory_ = NULL;

// The default SafeBrowsingServiceFactory.  Global, made a singleton so we
// don't leak it.
class SafeBrowsingServiceFactoryImpl : public SafeBrowsingServiceFactory {
 public:
  virtual SafeBrowsingService* CreateSafeBrowsingService() {
    return new SafeBrowsingService();
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<SafeBrowsingServiceFactoryImpl>;

  SafeBrowsingServiceFactoryImpl() { }

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceFactoryImpl);
};

static base::LazyInstance<SafeBrowsingServiceFactoryImpl>
    g_safe_browsing_service_factory_impl(base::LINKER_INITIALIZED);

struct SafeBrowsingService::WhiteListedEntry {
  int render_process_host_id;
  int render_view_id;
  std::string domain;
  UrlCheckResult result;
};

SafeBrowsingService::UnsafeResource::UnsafeResource()
    : resource_type(ResourceType::MAIN_FRAME),
      threat_type(URL_SAFE),
      client(NULL),
      render_process_host_id(-1),
      render_view_id(-1) {
}

SafeBrowsingService::UnsafeResource::~UnsafeResource() {}

SafeBrowsingService::SafeBrowsingCheck::SafeBrowsingCheck()
    : client(NULL),
      need_get_hash(false),
      result(URL_SAFE) {
}

SafeBrowsingService::SafeBrowsingCheck::~SafeBrowsingCheck() {}

/* static */
SafeBrowsingService* SafeBrowsingService::CreateSafeBrowsingService() {
  if (!factory_)
    factory_ = g_safe_browsing_service_factory_impl.Pointer();
  return factory_->CreateSafeBrowsingService();
}

SafeBrowsingService::SafeBrowsingService()
    : database_(NULL),
      protocol_manager_(NULL),
      enabled_(false),
      enable_download_protection_(false),
      update_in_progress_(false),
      database_update_in_progress_(false),
      closing_database_(false) {
}

void SafeBrowsingService::Initialize() {
  // Get the profile's preference for SafeBrowsing.
  PrefService* pref_service = GetDefaultProfile()->GetPrefs();
  if (pref_service->GetBoolean(prefs::kSafeBrowsingEnabled))
    Start();
}

void SafeBrowsingService::ShutDown() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SafeBrowsingService::OnIOShutdown));
}

bool SafeBrowsingService::CanCheckUrl(const GURL& url) const {
  return url.SchemeIs(chrome::kFtpScheme) ||
         url.SchemeIs(chrome::kHttpScheme) ||
         url.SchemeIs(chrome::kHttpsScheme);
}

// Only report SafeBrowsing related stats when UMA is enabled and
// safe browsing is enabled.
bool SafeBrowsingService::CanReportStats() const {
  const MetricsService* metrics = g_browser_process->metrics_service();
  const PrefService* pref_service = GetDefaultProfile()->GetPrefs();
  return metrics && metrics->reporting_active() &&
      pref_service && pref_service->GetBoolean(prefs::kSafeBrowsingEnabled);
}

// Binhash verification is only enabled for UMA users for now.
bool SafeBrowsingService::DownloadBinHashNeeded() const {
  return enable_download_protection_ && CanReportStats();
}

void SafeBrowsingService::CheckDownloadUrlDone(
    SafeBrowsingCheck* check, UrlCheckResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enable_download_protection_);
  VLOG(1) << "CheckDownloadUrlDone: " << result;

  if (checks_.find(check) == checks_.end() || !check->client)
    return;
  check->client->OnSafeBrowsingResult(check->url, result);
  checks_.erase(check);
}

void SafeBrowsingService::CheckDownloadUrlOnSBThread(SafeBrowsingCheck* check) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  DCHECK(enable_download_protection_);

  std::vector<SBPrefix> prefix_hits;

  if (!database_->ContainsDownloadUrl(check->url, &prefix_hits)) {
    // Good, we don't have hash for this url prefix.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &SafeBrowsingService::CheckDownloadUrlDone,
                          check, URL_SAFE));
    return;
  }

  check->need_get_hash = true;
  check->prefix_hits.swap(prefix_hits);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SafeBrowsingService::OnCheckDone, check));
}

bool SafeBrowsingService::CheckDownloadUrl(const GURL& url,
                                           Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_ || !enable_download_protection_)
    return true;

  // We need to check the database for url prefix, and later may fetch the url
  // from the safebrowsing backends. These need to be asynchronous.
  SafeBrowsingCheck* check = new SafeBrowsingCheck();

  check->url = url;
  check->client = client;
  check->result = URL_SAFE;
  checks_.insert(check);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::CheckDownloadUrlOnSBThread, check));

  return false;
}

bool SafeBrowsingService::CheckBrowseUrl(const GURL& url,
                                         Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_)
    return true;

  if (!CanCheckUrl(url))
    return true;

  if (!MakeDatabaseAvailable()) {
    QueuedCheck check;
    check.client = client;
    check.url = url;
    queued_checks_.push_back(check);
    return false;
  }

  std::string list;
  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> full_hits;
  base::Time check_start = base::Time::Now();
  bool prefix_match =
      database_->ContainsBrowseUrl(url, &list, &prefix_hits,
                                   &full_hits,
                                   protocol_manager_->last_update());

  UMA_HISTOGRAM_TIMES("SB2.FilterCheck", base::Time::Now() - check_start);

  if (!prefix_match)
    return true;  // URL is okay.

  // Needs to be asynchronous, since we could be in the constructor of a
  // ResourceDispatcherHost event handler which can't pause there.
  SafeBrowsingCheck* check = new SafeBrowsingCheck();
  check->url = url;
  check->client = client;
  check->result = URL_SAFE;
  check->need_get_hash = full_hits.empty();
  check->prefix_hits.swap(prefix_hits);
  check->full_hits.swap(full_hits);
  checks_.insert(check);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SafeBrowsingService::OnCheckDone, check));

  return false;
}

void SafeBrowsingService::CancelCheck(Client* client) {
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

void SafeBrowsingService::DisplayBlockingPage(const GURL& url,
                                              const GURL& original_url,
                                              ResourceType::Type resource_type,
                                              UrlCheckResult result,
                                              Client* client,
                                              int render_process_host_id,
                                              int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Check if the user has already ignored our warning for this render_view
  // and domain.
  for (size_t i = 0; i < white_listed_entries_.size(); ++i) {
    const WhiteListedEntry& entry = white_listed_entries_[i];
    if (entry.render_process_host_id == render_process_host_id &&
        entry.render_view_id == render_view_id &&
        entry.result == result &&
        entry.domain ==
        net::RegistryControlledDomainService::GetDomainAndRegistry(url)) {
      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &SafeBrowsingService::NotifyClientBlockingComplete,
          client, true));
      return;
    }
  }

  UnsafeResource resource;
  resource.url = url;
  resource.original_url = original_url;
  resource.resource_type = resource_type;
  resource.threat_type= result;
  resource.client = client;
  resource.render_process_host_id = render_process_host_id;
  resource.render_view_id = render_view_id;

  // The blocking page must be created from the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &SafeBrowsingService::DoDisplayBlockingPage, resource));
}

void SafeBrowsingService::HandleGetHashResults(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes,
    bool can_cache) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (checks_.find(check) == checks_.end())
    return;

  DCHECK(enabled_);

  UMA_HISTOGRAM_LONG_TIMES("SB2.Network", Time::Now() - check->start);

  std::vector<SBPrefix> prefixes = check->prefix_hits;
  OnHandleGetHashResults(check, full_hashes);  // 'check' is deleted here.

  if (can_cache && MakeDatabaseAvailable()) {
    // Cache the GetHash results in memory:
    database_->CacheHashResults(prefixes, full_hashes);
  }
}

void SafeBrowsingService::HandleChunk(const std::string& list,
                                      SBChunkList* chunks) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::HandleChunkForDatabase, list, chunks));
}

void SafeBrowsingService::HandleChunkDelete(
    std::vector<SBChunkDelete>* chunk_deletes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::DeleteChunks, chunk_deletes));
}

void SafeBrowsingService::UpdateStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  DCHECK(!update_in_progress_);
  update_in_progress_ = true;
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::GetAllChunksFromDatabase));
}

void SafeBrowsingService::UpdateFinished(bool update_succeeded) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  if (update_in_progress_) {
    update_in_progress_ = false;
    safe_browsing_thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &SafeBrowsingService::DatabaseUpdateFinished,
                          update_succeeded));
  }
}

bool SafeBrowsingService::IsUpdateInProgress() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return update_in_progress_;
}

void SafeBrowsingService::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed) {
  for (std::vector<UnsafeResource>::const_iterator iter = resources.begin();
       iter != resources.end(); ++iter) {
    const UnsafeResource& resource = *iter;
    NotifyClientBlockingComplete(resource.client, proceed);

    if (proceed) {
      // Whitelist this domain and warning type for the given tab.
      WhiteListedEntry entry;
      entry.render_process_host_id = resource.render_process_host_id;
      entry.render_view_id = resource.render_view_id;
      entry.domain = net::RegistryControlledDomainService::GetDomainAndRegistry(
            resource.url);
      entry.result = resource.threat_type;
      white_listed_entries_.push_back(entry);
    }
  }
}

void SafeBrowsingService::OnNewMacKeys(const std::string& client_key,
                                       const std::string& wrapped_key) {
  PrefService* prefs = g_browser_process->local_state();
  if (prefs) {
    prefs->SetString(prefs::kSafeBrowsingClientKey, client_key);
    prefs->SetString(prefs::kSafeBrowsingWrappedKey, wrapped_key);
  }
}

void SafeBrowsingService::OnEnable(bool enabled) {
  if (enabled)
    Start();
  else
    ShutDown();
}

// static
void SafeBrowsingService::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kSafeBrowsingClientKey, "");
  prefs->RegisterStringPref(prefs::kSafeBrowsingWrappedKey, "");
}

void SafeBrowsingService::CloseDatabase() {
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
        NewRunnableMethod(this, &SafeBrowsingService::OnCloseDatabase));
  }
}

void SafeBrowsingService::ResetDatabase() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::OnResetDatabase));
}

void SafeBrowsingService::LogPauseDelay(TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SB2.Delay", time);
}

SafeBrowsingService::~SafeBrowsingService() {
  // We should have already been shut down.  If we're still enabled, then the
  // database isn't going to be closed properly, which could lead to corruption.
  DCHECK(!enabled_);
}

void SafeBrowsingService::OnIOInitialize(
    const std::string& client_key,
    const std::string& wrapped_key,
    URLRequestContextGetter* request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  enabled_ = true;

  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  enable_download_protection_ =
      cmdline->HasSwitch(switches::kSbEnableDownloadProtection);

  MakeDatabaseAvailable();

  // On Windows, get the safe browsing client name from the browser
  // distribution classes in installer util. These classes don't yet have
  // an analog on non-Windows builds so just keep the name specified here.
#if defined(OS_WIN)
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::string client_name(dist->GetSafeBrowsingName());
#else
#if defined(GOOGLE_CHROME_BUILD)
  std::string client_name("googlechrome");
#else
  std::string client_name("chromium");
#endif
#endif
  bool disable_auto_update =
      cmdline->HasSwitch(switches::kSbDisableAutoUpdate) ||
      cmdline->HasSwitch(switches::kDisableBackgroundNetworking);
  std::string info_url_prefix =
      cmdline->HasSwitch(switches::kSbInfoURLPrefix) ?
      cmdline->GetSwitchValueASCII(switches::kSbInfoURLPrefix) :
      kSbDefaultInfoURLPrefix;
  std::string mackey_url_prefix =
      cmdline->HasSwitch(switches::kSbMacKeyURLPrefix) ?
      cmdline->GetSwitchValueASCII(switches::kSbMacKeyURLPrefix) :
      kSbDefaultMacKeyURLPrefix;

  DCHECK(!protocol_manager_);
  protocol_manager_ =
      SafeBrowsingProtocolManager::Create(this,
                                          client_name,
                                          client_key,
                                          wrapped_key,
                                          request_context_getter,
                                          info_url_prefix,
                                          mackey_url_prefix,
                                          disable_auto_update);

  protocol_manager_->Initialize();
}

void SafeBrowsingService::OnIOShutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_)
    return;

  enabled_ = false;

  // This cancels all in-flight GetHash requests.
  delete protocol_manager_;
  protocol_manager_ = NULL;

  // Delete queued checks, calling back any clients with 'URL_SAFE'.
  // If we don't do this here we may fail to close the database below.
  while (!queued_checks_.empty()) {
    QueuedCheck check = queued_checks_.front();
    if (check.client)
      check.client->OnSafeBrowsingResult(check.url, URL_SAFE);
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
  safe_browsing_thread_.reset();

  // Delete pending checks, calling back any clients with 'URL_SAFE'.  We have
  // to do this after the db thread returns because methods on it can have
  // copies of these pointers, so deleting them might lead to accessing garbage.
  for (CurrentChecks::iterator it = checks_.begin();
       it != checks_.end(); ++it) {
    if ((*it)->client)
      (*it)->client->OnSafeBrowsingResult((*it)->url, URL_SAFE);
    delete *it;
  }
  checks_.clear();

  gethash_requests_.clear();
}

bool SafeBrowsingService::DatabaseAvailable() const {
  base::AutoLock lock(database_lock_);
  return !closing_database_ && (database_ != NULL);
}

bool SafeBrowsingService::MakeDatabaseAvailable() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(enabled_);
  if (DatabaseAvailable())
    return true;
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &SafeBrowsingService::GetDatabase));
  return false;
}

SafeBrowsingDatabase* SafeBrowsingService::GetDatabase() {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  if (database_)
    return database_;

  FilePath path;
  bool result = PathService::Get(chrome::DIR_USER_DATA, &path);
  DCHECK(result);
  path = path.Append(chrome::kSafeBrowsingBaseFilename);

  Time before = Time::Now();

  SafeBrowsingDatabase* database =
      SafeBrowsingDatabase::Create(enable_download_protection_);

  database->Init(path);
  {
    // Acquiring the lock here guarantees correct ordering between the writes to
    // the new database object above, and the setting of |databse_| below.
    base::AutoLock lock(database_lock_);
    database_ = database;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SafeBrowsingService::DatabaseLoadComplete));

  UMA_HISTOGRAM_TIMES("SB2.DatabaseOpen", Time::Now() - before);
  return database_;
}

void SafeBrowsingService::OnCheckDone(SafeBrowsingCheck* check) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If we've been shutdown during the database lookup, this check will already
  // have been deleted (in OnIOShutdown).
  if (!enabled_ || checks_.find(check) == checks_.end())
    return;

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
    check->start = Time::Now();
    protocol_manager_->GetFullHash(check, check->prefix_hits);
  } else {
    // We may have cached results for previous GetHash queries.  Since
    // this data comes from cache, don't histogram hits.
    HandleOneCheck(check, check->full_hits);
  }
}

void SafeBrowsingService::GetAllChunksFromDatabase() {
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
      NewRunnableMethod(
          this, &SafeBrowsingService::OnGetAllChunksFromDatabase, lists,
          database_error));
}

void SafeBrowsingService::OnGetAllChunksFromDatabase(
    const std::vector<SBListChunkRanges>& lists, bool database_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (enabled_)
    protocol_manager_->OnGetChunksComplete(lists, database_error);
}

void SafeBrowsingService::OnChunkInserted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (enabled_)
    protocol_manager_->OnChunkInserted();
}

void SafeBrowsingService::DatabaseLoadComplete() {
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
    HISTOGRAM_TIMES("SB.QueueDelay", Time::Now() - check.start);
    // If CheckUrl() determines the URL is safe immediately, it doesn't call the
    // client's handler function (because normally it's being directly called by
    // the client).  Since we're not the client, we have to convey this result.
    if (check.client && CheckBrowseUrl(check.url, check.client))
      check.client->OnSafeBrowsingResult(check.url, URL_SAFE);
    queued_checks_.pop_front();
  }
}

void SafeBrowsingService::HandleChunkForDatabase(
    const std::string& list_name, SBChunkList* chunks) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  if (chunks) {
    GetDatabase()->InsertChunks(list_name, *chunks);
    delete chunks;
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SafeBrowsingService::OnChunkInserted));
}

void SafeBrowsingService::DeleteChunks(
    std::vector<SBChunkDelete>* chunk_deletes) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  if (chunk_deletes) {
    GetDatabase()->DeleteChunks(*chunk_deletes);
    delete chunk_deletes;
  }
}

SafeBrowsingService::UrlCheckResult SafeBrowsingService::GetResultFromListname(
    const std::string& list_name) {
  if (safe_browsing_util::IsPhishingList(list_name)) {
    return URL_PHISHING;
  }

  if (safe_browsing_util::IsMalwareList(list_name)) {
    return URL_MALWARE;
  }

  if (safe_browsing_util::IsBadbinurlList(list_name)) {
    return BINARY_MALWARE;
  }

  DVLOG(1) << "Unknown safe browsing list " << list_name;
  return URL_SAFE;
}

void SafeBrowsingService::NotifyClientBlockingComplete(Client* client,
                                                       bool proceed) {
  client->OnBlockingPageComplete(proceed);
}

void SafeBrowsingService::DatabaseUpdateFinished(bool update_succeeded) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  GetDatabase()->UpdateFinished(update_succeeded);
  DCHECK(database_update_in_progress_);
  database_update_in_progress_ = false;
}

void SafeBrowsingService::Start() {
  DCHECK(!safe_browsing_thread_.get());
  safe_browsing_thread_.reset(new base::Thread("Chrome_SafeBrowsingThread"));
  if (!safe_browsing_thread_->Start())
    return;

  // Retrieve client MAC keys.
  PrefService* local_state = g_browser_process->local_state();
  std::string client_key, wrapped_key;
  if (local_state) {
    client_key =
      local_state->GetString(prefs::kSafeBrowsingClientKey);
    wrapped_key =
      local_state->GetString(prefs::kSafeBrowsingWrappedKey);
  }

  // We will issue network fetches using the default profile's request context.
  scoped_refptr<URLRequestContextGetter> request_context_getter(
      GetDefaultProfile()->GetRequestContext());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &SafeBrowsingService::OnIOInitialize, client_key, wrapped_key,
          request_context_getter));
}

void SafeBrowsingService::OnCloseDatabase() {
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

void SafeBrowsingService::OnResetDatabase() {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  GetDatabase()->ResetDatabase();
}

void SafeBrowsingService::CacheHashResults(
  const std::vector<SBPrefix>& prefixes,
  const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK_EQ(MessageLoop::current(), safe_browsing_thread_->message_loop());
  GetDatabase()->CacheHashResults(prefixes, full_hashes);
}

void SafeBrowsingService::OnHandleGetHashResults(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SBPrefix prefix = check->prefix_hits[0];
  GetHashRequests::iterator it = gethash_requests_.find(prefix);
  if (check->prefix_hits.size() > 1 || it == gethash_requests_.end()) {
    const bool hit = HandleOneCheck(check, full_hashes);
    RecordGetHashCheckStatus(hit, full_hashes);
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
  RecordGetHashCheckStatus(hit, full_hashes);

  gethash_requests_.erase(it);
}

bool SafeBrowsingService::HandleOneCheck(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Always calculate the index, for recording hits.
  int index = safe_browsing_util::CompareFullHashes(check->url, full_hashes);

  // |client| is NULL if the request was cancelled.
  if (check->client) {
    UrlCheckResult result = URL_SAFE;
    if (index != -1)
      result = GetResultFromListname(full_hashes[index].list_name);

    // Let the client continue handling the original request.
    check->client->OnSafeBrowsingResult(check->url, result);
  }

  checks_.erase(check);
  delete check;

  return (index != -1);
}

void SafeBrowsingService::DoDisplayBlockingPage(
    const UnsafeResource& resource) {
  // The tab might have been closed.
  TabContents* wc =
      tab_util::GetTabContentsByID(resource.render_process_host_id,
                                   resource.render_view_id);

  if (!wc) {
    // The tab is gone and we did not have a chance at showing the interstitial.
    // Just act as "Don't Proceed" was chosen.
    std::vector<UnsafeResource> resources;
    resources.push_back(resource);
    BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &SafeBrowsingService::OnBlockingPageDone, resources, false));
    return;
  }

  // Report the malicious resource to the SafeBrowsing servers if the user has
  // opted in to reporting statistics.
  const MetricsService* metrics = g_browser_process->metrics_service();
  DCHECK(metrics);
  if (metrics && metrics->reporting_active() &&
      (resource.threat_type == SafeBrowsingService::URL_MALWARE ||
       resource.threat_type == SafeBrowsingService::URL_PHISHING)) {
    GURL page_url = wc->GetURL();
    GURL referrer_url;
    NavigationEntry* entry = wc->controller().GetActiveEntry();
    if (entry)
      referrer_url = entry->referrer();
    bool is_subresource = resource.resource_type != ResourceType::MAIN_FRAME;

    // When the malicious url is on the main frame, and resource.original_url
    // is not the same as the resource.url, that means we have a redirect from
    // resource.original_url to resource.url.
    // Also, at this point, page_url points to the _previous_ page that we
    // were on. We replace page_url with resource.original_url and referrer
    // with page_url.
    if (!is_subresource &&
        !resource.original_url.is_empty() &&
        resource.original_url != resource.url) {
      referrer_url = page_url;
      page_url = resource.original_url;
    }

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            this,
            &SafeBrowsingService::ReportSafeBrowsingHit,
            resource.url,
            page_url,
            referrer_url,
            is_subresource,
            resource.threat_type));
  }

  SafeBrowsingBlockingPage::ShowBlockingPage(this, resource);
}

// A safebrowsing hit is sent right after we create a blocking page,
// only for UMA users.
void SafeBrowsingService::ReportSafeBrowsingHit(
    const GURL& malicious_url,
    const GURL& page_url,
    const GURL& referrer_url,
    bool is_subresource,
    SafeBrowsingService::UrlCheckResult threat_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_)
    return;

  DVLOG(1) << "ReportSafeBrowsingHit: " << malicious_url << " " << page_url
           << " " << referrer_url << " " << is_subresource << " "
           << threat_type;
  protocol_manager_->ReportSafeBrowsingHit(malicious_url, page_url,
                                           referrer_url, is_subresource,
                                           threat_type);
}

// A MalwareDetails report is sent after the blocking page is going
// away, at which point we see if the user had opted-in using the
// checkbox on the blocking page.
void SafeBrowsingService::ReportMalwareDetails(
    scoped_refptr<MalwareDetails> details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  scoped_ptr<const std::string> serialized(details->GetSerializedReport());
  if (!serialized->empty()) {
    DVLOG(1) << "Sending serialized malware details.";
    protocol_manager_->ReportMalwareDetails(*serialized);
  }
}
