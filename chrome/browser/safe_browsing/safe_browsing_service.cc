// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "net/base/registry_controlled_domain.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#endif

using base::Time;
using base::TimeDelta;

static Profile* GetDefaultProfile() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetDefaultProfile(user_data_dir);
}

SafeBrowsingService::SafeBrowsingService()
    : database_(NULL),
      protocol_manager_(NULL),
      enabled_(false),
      update_in_progress_(false),
      closing_database_(false) {
}

void SafeBrowsingService::Initialize() {
  // Get the profile's preference for SafeBrowsing.
  PrefService* pref_service = GetDefaultProfile()->GetPrefs();
  if (pref_service->GetBoolean(prefs::kSafeBrowsingEnabled))
    Start();
}

void SafeBrowsingService::ShutDown() {
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SafeBrowsingService::OnIOShutdown));
}

bool SafeBrowsingService::CanCheckUrl(const GURL& url) const {
  return url.SchemeIs(chrome::kHttpScheme) ||
         url.SchemeIs(chrome::kHttpsScheme);
}

bool SafeBrowsingService::CheckUrl(const GURL& url, Client* client) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!enabled_)
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
  bool prefix_match = database_->ContainsUrl(url, &list, &prefix_hits,
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

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SafeBrowsingService::OnCheckDone, check));

  return false;
}

void SafeBrowsingService::CancelCheck(Client* client) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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
                                              ResourceType::Type resource_type,
                                              UrlCheckResult result,
                                              Client* client,
                                              int render_process_host_id,
                                              int render_view_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

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
  resource.resource_type = resource_type;
  resource.threat_type= result;
  resource.client = client;
  resource.render_process_host_id = render_process_host_id;
  resource.render_view_id = render_view_id;

  // The blocking page must be created from the UI thread.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &SafeBrowsingService::DoDisplayBlockingPage, resource));
}

void SafeBrowsingService::HandleGetHashResults(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes,
    bool can_cache) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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
                                      std::deque<SBChunk>* chunks) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(enabled_);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::HandleChunkForDatabase, list, chunks));
}

void SafeBrowsingService::HandleChunkDelete(
    std::vector<SBChunkDelete>* chunk_deletes) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(enabled_);
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::DeleteChunks, chunk_deletes));
}

void SafeBrowsingService::UpdateStarted() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(enabled_);
  DCHECK(!update_in_progress_);
  update_in_progress_ = true;
  safe_browsing_thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SafeBrowsingService::GetAllChunksFromDatabase));
}

void SafeBrowsingService::UpdateFinished(bool update_succeeded) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(enabled_);
  if (update_in_progress_) {
    update_in_progress_ = false;
    safe_browsing_thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &SafeBrowsingService::DatabaseUpdateFinished,
                          update_succeeded));
  }
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
    prefs->SetString(prefs::kSafeBrowsingClientKey, ASCIIToWide(client_key));
    prefs->SetString(prefs::kSafeBrowsingWrappedKey, ASCIIToWide(wrapped_key));
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
  prefs->RegisterStringPref(prefs::kSafeBrowsingClientKey, L"");
  prefs->RegisterStringPref(prefs::kSafeBrowsingWrappedKey, L"");
}

void SafeBrowsingService::CloseDatabase() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

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
  // The first two cases above are handled by checking database_available().
  if (!database_available() || !queued_checks_.empty())
    return;

  closing_database_ = true;
  if (safe_browsing_thread_.get()) {
    safe_browsing_thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &SafeBrowsingService::OnCloseDatabase));
  }
}

void SafeBrowsingService::ResetDatabase() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  enabled_ = true;
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

  protocol_manager_ = new SafeBrowsingProtocolManager(this,
                                                      client_name,
                                                      client_key,
                                                      wrapped_key,
                                                      request_context_getter);

  // Balance the reference added by Start().
  request_context_getter->Release();

  protocol_manager_->Initialize();
}

void SafeBrowsingService::OnIOShutdown() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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
      check.client->OnUrlCheckResult(check.url, URL_SAFE);
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
      (*it)->client->OnUrlCheckResult((*it)->url, URL_SAFE);
    delete *it;
  }
  checks_.clear();

  gethash_requests_.clear();
}

bool SafeBrowsingService::MakeDatabaseAvailable() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(enabled_);
  if (!database_) {
    DCHECK(!closing_database_);
    safe_browsing_thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &SafeBrowsingService::GetDatabase));
  }
  return database_available();
}

SafeBrowsingDatabase* SafeBrowsingService::GetDatabase() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  if (database_)
    return database_;

  FilePath path;
  bool result = PathService::Get(chrome::DIR_USER_DATA, &path);
  DCHECK(result);
  path = path.Append(chrome::kSafeBrowsingFilename);

  Time before = Time::Now();
  SafeBrowsingDatabase* database = SafeBrowsingDatabase::Create();
  Callback0::Type* chunk_callback =
      NewCallback(this, &SafeBrowsingService::ChunkInserted);
  database->Init(path, chunk_callback);
  database_ = database;

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SafeBrowsingService::DatabaseLoadComplete));

  UMA_HISTOGRAM_TIMES("SB2.DatabaseOpen", Time::Now() - before);
  return database_;
}

void SafeBrowsingService::OnCheckDone(SafeBrowsingCheck* check) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

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
    // We may have cached results for previous GetHash queries.
    HandleOneCheck(check, check->full_hits);
  }
}

void SafeBrowsingService::GetAllChunksFromDatabase() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  bool database_error = true;
  std::vector<SBListChunkRanges> lists;
  GetDatabase();  // This guarantees that |database_| is non-NULL.
  if (database_->UpdateStarted()) {
    database_->GetListsInfo(&lists);
    database_error = false;
  } else {
    database_->UpdateFinished(false);
  }

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &SafeBrowsingService::OnGetAllChunksFromDatabase, lists,
          database_error));
}

void SafeBrowsingService::OnGetAllChunksFromDatabase(
    const std::vector<SBListChunkRanges>& lists, bool database_error) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (enabled_)
    protocol_manager_->OnGetChunksComplete(lists, database_error);
}

void SafeBrowsingService::ChunkInserted() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SafeBrowsingService::OnChunkInserted));
}

void SafeBrowsingService::OnChunkInserted() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (enabled_)
    protocol_manager_->OnChunkInserted();
}

void SafeBrowsingService::DatabaseLoadComplete() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!enabled_)
    return;

  HISTOGRAM_COUNTS("SB.QueueDepth", queued_checks_.size());
  if (queued_checks_.empty())
    return;

  // If the database isn't already available, calling CheckUrl() in the loop
  // below will add the check back to the queue, and we'll infinite-loop.
  DCHECK(database_available());
  while (!queued_checks_.empty()) {
    QueuedCheck check = queued_checks_.front();
    HISTOGRAM_TIMES("SB.QueueDelay", Time::Now() - check.start);
    // If CheckUrl() determines the URL is safe immediately, it doesn't call the
    // client's handler function (because normally it's being directly called by
    // the client).  Since we're not the client, we have to convey this result.
    if (check.client && CheckUrl(check.url, check.client))
      check.client->OnUrlCheckResult(check.url, URL_SAFE);
    queued_checks_.pop_front();
  }
}

void SafeBrowsingService::HandleChunkForDatabase(
    const std::string& list_name,
    std::deque<SBChunk>* chunks) {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  GetDatabase()->InsertChunks(list_name, chunks);
}

void SafeBrowsingService::DeleteChunks(
    std::vector<SBChunkDelete>* chunk_deletes) {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  GetDatabase()->DeleteChunks(chunk_deletes);
}

SafeBrowsingService::UrlCheckResult SafeBrowsingService::GetResultFromListname(
    const std::string& list_name) {
  if (safe_browsing_util::IsPhishingList(list_name)) {
    return URL_PHISHING;
  }

  if (safe_browsing_util::IsMalwareList(list_name)) {
    return URL_MALWARE;
  }

  SB_DLOG(INFO) << "Unknown safe browsing list " << list_name;
  return URL_SAFE;
}

void SafeBrowsingService::NotifyClientBlockingComplete(Client* client,
                                                       bool proceed) {
  client->OnBlockingPageComplete(proceed);
}

void SafeBrowsingService::DatabaseUpdateFinished(bool update_succeeded) {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  GetDatabase()->UpdateFinished(update_succeeded);
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
      WideToASCII(local_state->GetString(prefs::kSafeBrowsingClientKey));
    wrapped_key =
      WideToASCII(local_state->GetString(prefs::kSafeBrowsingWrappedKey));
  }

  // We will issue network fetches using the default profile's request context.
  URLRequestContextGetter* request_context_getter =
      GetDefaultProfile()->GetRequestContext();
  request_context_getter->AddRef();  // Balanced in OnIOInitialize.

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &SafeBrowsingService::OnIOInitialize, client_key, wrapped_key,
          request_context_getter));
}

void SafeBrowsingService::OnCloseDatabase() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  DCHECK(closing_database_);

  // Because |closing_database_| is true, nothing on the IO thread will be
  // accessing the database, so it's safe to delete and then NULL the pointer.
  delete database_;
  database_ = NULL;
  closing_database_ = false;
}

void SafeBrowsingService::OnResetDatabase() {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  GetDatabase()->ResetDatabase();
}

void SafeBrowsingService::CacheHashResults(
  const std::vector<SBPrefix>& prefixes,
  const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK(MessageLoop::current() == safe_browsing_thread_->message_loop());
  GetDatabase()->CacheHashResults(prefixes, full_hashes);
}

void SafeBrowsingService::OnHandleGetHashResults(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  SBPrefix prefix = check->prefix_hits[0];
  GetHashRequests::iterator it = gethash_requests_.find(prefix);
  if (check->prefix_hits.size() > 1 || it == gethash_requests_.end()) {
    HandleOneCheck(check, full_hashes);
    return;
  }

  // Call back all interested parties.
  GetHashRequestors& requestors = it->second;
  for (GetHashRequestors::iterator r = requestors.begin();
       r != requestors.end(); ++r) {
    HandleOneCheck(*r, full_hashes);
  }

  gethash_requests_.erase(it);
}

void SafeBrowsingService::HandleOneCheck(
    SafeBrowsingCheck* check,
    const std::vector<SBFullHashResult>& full_hashes) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (check->client) {
    UrlCheckResult result = URL_SAFE;
    int index = safe_browsing_util::CompareFullHashes(check->url, full_hashes);
    if (index != -1) {
      result = GetResultFromListname(full_hashes[index].list_name);
    } else {
      // Log the case where the SafeBrowsing servers return full hashes in the
      // GetHash response that match the prefix we're looking up, but don't
      // match the full hash of the URL.
      if (!full_hashes.empty())
        UMA_HISTOGRAM_COUNTS("SB2.GetHashServerMiss", 1);
    }

    // Let the client continue handling the original request.
    check->client->OnUrlCheckResult(check->url, result);
  }

  checks_.erase(check);
  delete check;
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
    ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &SafeBrowsingService::OnBlockingPageDone, resources, false));
    return;
  }

  // Report the malware sub-resource to the SafeBrowsing servers if we have a
  // malware sub-resource on a safe page and only if the user has opted in to
  // reporting statistics.
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);
  if (prefs && prefs->GetBoolean(prefs::kMetricsReportingEnabled) &&
      resource.resource_type != ResourceType::MAIN_FRAME &&
      resource.threat_type == SafeBrowsingService::URL_MALWARE) {
    GURL page_url = wc->GetURL();
    GURL referrer_url;
    NavigationEntry* entry = wc->controller().GetActiveEntry();
    if (entry)
      referrer_url = entry->referrer();
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &SafeBrowsingService::ReportMalware,
                          resource.url,
                          page_url,
                          referrer_url));
  }

  SafeBrowsingBlockingPage::ShowBlockingPage(this, resource);
}

void SafeBrowsingService::ReportMalware(const GURL& malware_url,
                                        const GURL& page_url,
                                        const GURL& referrer_url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (!enabled_)
    return;

  if (database_) {
    // Check if 'page_url' is already blacklisted (exists in our cache). Only
    // report if it's not there.
    std::string list;
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> full_hits;
    database_->ContainsUrl(page_url, &list, &prefix_hits, &full_hits,
                           protocol_manager_->last_update());
    if (!full_hits.empty())
      return;
  }

  protocol_manager_->ReportMalware(malware_url, page_url, referrer_url);
}
