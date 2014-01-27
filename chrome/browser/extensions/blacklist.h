// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLACKLIST_H_
#define CHROME_BROWSER_EXTENSIONS_BLACKLIST_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/blacklist_state.h"

namespace extensions {

class BlacklistStateFetcher;
class Extension;
class ExtensionPrefs;

// The blacklist of extensions backed by safe browsing.
class Blacklist : public content::NotificationObserver,
                  public base::SupportsWeakPtr<Blacklist> {
 public:
  class Observer {
   public:
    // Observes |blacklist| on construction and unobserves on destruction.
    explicit Observer(Blacklist* blacklist);

    virtual void OnBlacklistUpdated() = 0;

   protected:
    virtual ~Observer();

   private:
    Blacklist* blacklist_;
  };

  class ScopedDatabaseManagerForTest {
   public:
    explicit ScopedDatabaseManagerForTest(
        scoped_refptr<SafeBrowsingDatabaseManager> database_manager);

    ~ScopedDatabaseManagerForTest();

   private:
    scoped_refptr<SafeBrowsingDatabaseManager> original_;

    DISALLOW_COPY_AND_ASSIGN(ScopedDatabaseManagerForTest);
  };

  typedef std::map<std::string, BlacklistState> BlacklistStateMap;

  typedef base::Callback<void(const BlacklistStateMap&)>
      GetBlacklistedIDsCallback;

  typedef base::Callback<void(const std::set<std::string>&)>
      GetMalwareIDsCallback;

  typedef base::Callback<void(BlacklistState)> IsBlacklistedCallback;

  explicit Blacklist(ExtensionPrefs* prefs);

  virtual ~Blacklist();

  // From the set of extension IDs passed in via |ids|, asynchronously checks
  // which are blacklisted and includes them in the resulting map passed
  // via |callback|, which will be sent on the caller's message loop. The values
  // of the map are the blacklist state for each extension. Extensions with
  // a BlacklistState of NOT_BLACKLISTED are not included in the result.
  //
  // For a synchronous version which ONLY CHECKS CURRENTLY INSTALLED EXTENSIONS
  // see ExtensionPrefs::IsExtensionBlacklisted.
  void GetBlacklistedIDs(const std::set<std::string>& ids,
                         const GetBlacklistedIDsCallback& callback);

  // From the subset of extension IDs passed in via |ids|, select the ones
  // marked in the blacklist as BLACKLISTED_MALWARE and asynchronously pass
  // to |callback|. Basically, will call GetBlacklistedIDs and filter its
  // results.
  void GetMalwareIDs(const std::set<std::string>& ids,
                     const GetMalwareIDsCallback& callback);

  // More convenient form of GetBlacklistedIDs for checking a single extension.
  void IsBlacklisted(const std::string& extension_id,
                     const IsBlacklistedCallback& callback);

  // Used to mock BlacklistStateFetcher in unit tests. Blacklist owns the
  // |fetcher|.
  void SetBlacklistStateFetcherForTest(BlacklistStateFetcher* fetcher);

  // Reset the owned BlacklistStateFetcher to null and return the current
  // BlacklistStateFetcher.
  BlacklistStateFetcher* ResetBlacklistStateFetcherForTest();

  // Adds/removes an observer to the blacklist.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Use via ScopedDatabaseManagerForTest.
  static void SetDatabaseManager(
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager);
  static scoped_refptr<SafeBrowsingDatabaseManager> GetDatabaseManager();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void GetBlacklistStateForIDs(const GetBlacklistedIDsCallback& callback,
                               const std::set<std::string>& blacklisted_ids);

  void RequestExtensionsBlacklistState(const std::set<std::string>& ids,
                                       const base::Callback<void()>& callback);

  void OnBlacklistStateReceived(const std::string& id, BlacklistState state);

  void ReturnBlacklistStateMap(const GetBlacklistedIDsCallback& callback,
                               const std::set<std::string>& blacklisted_ids);

  ObserverList<Observer> observers_;

  content::NotificationRegistrar registrar_;

  // The cached BlacklistState's, received from BlacklistStateFetcher.
  BlacklistStateMap blacklist_state_cache_;

  scoped_ptr<BlacklistStateFetcher> state_fetcher_;

  typedef std::list<std::pair<std::vector<std::string>,
                              base::Callback<void()> > >
      StateRequestsList;

  // The list of ongoing requests for blacklist states that couldn't be
  // served directly from the cache. A new request is created in
  // GetBlacklistedIDs and deleted when the callback is called from
  // OnBlacklistStateReceived.
  StateRequestsList state_requests_;

  DISALLOW_COPY_AND_ASSIGN(Blacklist);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLACKLIST_H_
