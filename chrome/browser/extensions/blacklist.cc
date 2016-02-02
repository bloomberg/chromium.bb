// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/blacklist_factory.h"
#include "chrome/browser/extensions/blacklist_state_fetcher.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_prefs.h"

using content::BrowserThread;
using safe_browsing::SafeBrowsingDatabaseManager;

namespace extensions {

namespace {

// The safe browsing database manager to use. Make this a global/static variable
// rather than a member of Blacklist because Blacklist accesses the real
// database manager before it has a chance to get a fake one.
class LazySafeBrowsingDatabaseManager {
 public:
  LazySafeBrowsingDatabaseManager() {
#if defined(SAFE_BROWSING_DB_LOCAL)
    if (g_browser_process && g_browser_process->safe_browsing_service()) {
      instance_ =
          g_browser_process->safe_browsing_service()->database_manager();
    }
#endif
  }

  scoped_refptr<SafeBrowsingDatabaseManager> get() {
    return instance_;
  }

  void set(scoped_refptr<SafeBrowsingDatabaseManager> instance) {
    instance_ = instance;
  }

 private:
  scoped_refptr<SafeBrowsingDatabaseManager> instance_;
};

static base::LazyInstance<LazySafeBrowsingDatabaseManager> g_database_manager =
    LAZY_INSTANCE_INITIALIZER;

// Implementation of SafeBrowsingDatabaseManager::Client, the class which is
// called back from safebrowsing queries.
//
// Constructed on any thread but lives on the IO from then on.
class SafeBrowsingClientImpl
    : public SafeBrowsingDatabaseManager::Client,
      public base::RefCountedThreadSafe<SafeBrowsingClientImpl> {
 public:
  typedef base::Callback<void(const std::set<std::string>&)> OnResultCallback;

  // Constructs a client to query the database manager for |extension_ids| and
  // run |callback| with the IDs of those which have been blacklisted.
  SafeBrowsingClientImpl(
      const std::set<std::string>& extension_ids,
      const OnResultCallback& callback)
      : callback_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        callback_(callback) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SafeBrowsingClientImpl::StartCheck, this,
                   g_database_manager.Get().get(),
                   extension_ids));
  }

 private:
  friend class base::RefCountedThreadSafe<SafeBrowsingClientImpl>;
  ~SafeBrowsingClientImpl() override {}

  // Pass |database_manager| as a parameter to avoid touching
  // SafeBrowsingService on the IO thread.
  void StartCheck(scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
                  const std::set<std::string>& extension_ids) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (database_manager->CheckExtensionIDs(extension_ids, this)) {
      // Definitely not blacklisted. Callback immediately.
      callback_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(callback_, std::set<std::string>()));
      return;
    }
    // Something might be blacklisted, response will come in
    // OnCheckExtensionsResult.
    AddRef();  // Balanced in OnCheckExtensionsResult
  }

  void OnCheckExtensionsResult(const std::set<std::string>& hits) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    callback_task_runner_->PostTask(FROM_HERE, base::Bind(callback_, hits));
    Release();  // Balanced in StartCheck.
  }

  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
  OnResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingClientImpl);
};

void CheckOneExtensionState(
    const Blacklist::IsBlacklistedCallback& callback,
    const Blacklist::BlacklistStateMap& state_map) {
  callback.Run(state_map.empty() ? NOT_BLACKLISTED : state_map.begin()->second);
}

void GetMalwareFromBlacklistStateMap(
    const Blacklist::GetMalwareIDsCallback& callback,
    const Blacklist::BlacklistStateMap& state_map) {
  std::set<std::string> malware;
  for (Blacklist::BlacklistStateMap::const_iterator it = state_map.begin();
       it != state_map.end(); ++it) {
    // TODO(oleg): UNKNOWN is treated as MALWARE for backwards compatibility.
    // In future GetMalwareIDs will be removed and the caller will have to
    // deal with BLACKLISTED_UNKNOWN state returned from GetBlacklistedIDs.
    if (it->second == BLACKLISTED_MALWARE || it->second == BLACKLISTED_UNKNOWN)
      malware.insert(it->first);
  }
  callback.Run(malware);
}

}  // namespace

Blacklist::Observer::Observer(Blacklist* blacklist) : blacklist_(blacklist) {
  blacklist_->AddObserver(this);
}

Blacklist::Observer::~Observer() {
  blacklist_->RemoveObserver(this);
}

Blacklist::ScopedDatabaseManagerForTest::ScopedDatabaseManagerForTest(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager)
    : original_(GetDatabaseManager()) {
  SetDatabaseManager(database_manager);
}

Blacklist::ScopedDatabaseManagerForTest::~ScopedDatabaseManagerForTest() {
  SetDatabaseManager(original_);
}

Blacklist::Blacklist(ExtensionPrefs* prefs) {
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager =
      g_database_manager.Get().get();
  if (database_manager.get()) {
    registrar_.Add(
        this, chrome::NOTIFICATION_SAFE_BROWSING_UPDATE_COMPLETE,
        content::Source<SafeBrowsingDatabaseManager>(database_manager.get()));
  }

  // Clear out the old prefs-backed blacklist, stored as empty extension entries
  // with just a "blacklisted" property.
  //
  // TODO(kalman): Delete this block of code, see http://crbug.com/295882.
  std::set<std::string> blacklisted = prefs->GetBlacklistedExtensions();
  for (std::set<std::string>::iterator it = blacklisted.begin();
       it != blacklisted.end(); ++it) {
    if (!prefs->GetInstalledExtensionInfo(*it))
      prefs->DeleteExtensionPrefs(*it);
  }
}

Blacklist::~Blacklist() {
}

// static
Blacklist* Blacklist::Get(content::BrowserContext* context) {
  return BlacklistFactory::GetForBrowserContext(context);
}

void Blacklist::GetBlacklistedIDs(const std::set<std::string>& ids,
                                  const GetBlacklistedIDsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (ids.empty() || !g_database_manager.Get().get().get()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BlacklistStateMap()));
    return;
  }

  // Constructing the SafeBrowsingClientImpl begins the process of asking
  // safebrowsing for the blacklisted extensions. The set of blacklisted
  // extensions returned by SafeBrowsing will then be passed to
  // GetBlacklistStateIDs to get the particular BlacklistState for each id.
  new SafeBrowsingClientImpl(
      ids, base::Bind(&Blacklist::GetBlacklistStateForIDs, AsWeakPtr(),
                      callback));
}

void Blacklist::GetMalwareIDs(const std::set<std::string>& ids,
                              const GetMalwareIDsCallback& callback) {
  GetBlacklistedIDs(ids, base::Bind(&GetMalwareFromBlacklistStateMap,
                                    callback));
}


void Blacklist::IsBlacklisted(const std::string& extension_id,
                              const IsBlacklistedCallback& callback) {
  std::set<std::string> check;
  check.insert(extension_id);
  GetBlacklistedIDs(check, base::Bind(&CheckOneExtensionState, callback));
}

void Blacklist::GetBlacklistStateForIDs(
    const GetBlacklistedIDsCallback& callback,
    const std::set<std::string>& blacklisted_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::set<std::string> ids_unknown_state;
  BlacklistStateMap extensions_state;
  for (std::set<std::string>::const_iterator it = blacklisted_ids.begin();
       it != blacklisted_ids.end(); ++it) {
    BlacklistStateMap::const_iterator cache_it =
        blacklist_state_cache_.find(*it);
    if (cache_it == blacklist_state_cache_.end() ||
        cache_it->second == BLACKLISTED_UNKNOWN)  // Do not return UNKNOWN
                                                  // from cache, retry request.
      ids_unknown_state.insert(*it);
    else
      extensions_state[*it] = cache_it->second;
  }

  if (ids_unknown_state.empty()) {
    callback.Run(extensions_state);
  } else {
    // After the extension blacklist states have been downloaded, call this
    // functions again, but prevent infinite cycle in case server is offline
    // or some other reason prevents us from receiving the blacklist state for
    // these extensions.
    RequestExtensionsBlacklistState(
        ids_unknown_state,
        base::Bind(&Blacklist::ReturnBlacklistStateMap, AsWeakPtr(),
                   callback, blacklisted_ids));
  }
}

void Blacklist::ReturnBlacklistStateMap(
    const GetBlacklistedIDsCallback& callback,
    const std::set<std::string>& blacklisted_ids) {
  BlacklistStateMap extensions_state;
  for (std::set<std::string>::const_iterator it = blacklisted_ids.begin();
       it != blacklisted_ids.end(); ++it) {
    BlacklistStateMap::const_iterator cache_it =
        blacklist_state_cache_.find(*it);
    if (cache_it != blacklist_state_cache_.end())
      extensions_state[*it] = cache_it->second;
    // If for some reason we still haven't cached the state of this extension,
    // we silently skip it.
  }

  callback.Run(extensions_state);
}

void Blacklist::RequestExtensionsBlacklistState(
    const std::set<std::string>& ids, const base::Callback<void()>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!state_fetcher_)
    state_fetcher_.reset(new BlacklistStateFetcher());

  state_requests_.push_back(
      make_pair(std::vector<std::string>(ids.begin(), ids.end()), callback));
  for (std::set<std::string>::const_iterator it = ids.begin();
       it != ids.end();
       ++it) {
    state_fetcher_->Request(
        *it,
        base::Bind(&Blacklist::OnBlacklistStateReceived, AsWeakPtr(), *it));
  }
}

void Blacklist::OnBlacklistStateReceived(const std::string& id,
                                         BlacklistState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blacklist_state_cache_[id] = state;

  // Go through the opened requests and call the callbacks for those requests
  // for which we already got all the required blacklist states.
  StateRequestsList::iterator requests_it = state_requests_.begin();
  while (requests_it != state_requests_.end()) {
    const std::vector<std::string>& ids = requests_it->first;

    bool have_all_in_cache = true;
    for (std::vector<std::string>::const_iterator ids_it = ids.begin();
         ids_it != ids.end();
         ++ids_it) {
      if (!ContainsKey(blacklist_state_cache_, *ids_it)) {
        have_all_in_cache = false;
        break;
      }
    }

    if (have_all_in_cache) {
      requests_it->second.Run();
      requests_it = state_requests_.erase(requests_it); // returns next element
    } else {
      ++requests_it;
    }
  }
}

void Blacklist::SetBlacklistStateFetcherForTest(
    BlacklistStateFetcher* fetcher) {
  state_fetcher_.reset(fetcher);
}

BlacklistStateFetcher* Blacklist::ResetBlacklistStateFetcherForTest() {
  return state_fetcher_.release();
}

void Blacklist::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void Blacklist::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

// static
void Blacklist::SetDatabaseManager(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager) {
  g_database_manager.Get().set(database_manager);
}

// static
scoped_refptr<SafeBrowsingDatabaseManager> Blacklist::GetDatabaseManager() {
  return g_database_manager.Get().get();
}

void Blacklist::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_SAFE_BROWSING_UPDATE_COMPLETE, type);
  FOR_EACH_OBSERVER(Observer, observers_, OnBlacklistUpdated());
}

}  // namespace extensions
