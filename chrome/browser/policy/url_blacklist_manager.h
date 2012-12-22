// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_URL_BLACKLIST_MANAGER_H_
#define CHROME_BROWSER_POLICY_URL_BLACKLIST_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/common/extensions/matcher/url_matcher.h"

class GURL;
class PrefService;
class PrefServiceSyncable;

namespace base {
class ListValue;
}

namespace policy {

// Contains a set of filters to block and allow certain URLs, and matches GURLs
// against this set. The filters are currently kept in memory.
class URLBlacklist {
 public:
  URLBlacklist();
  virtual ~URLBlacklist();

  // Allows or blocks URLs matching one of the filters, depending on |allow|.
  void AddFilters(bool allow, const base::ListValue* filters);

  // URLs matching one of the |filters| will be blocked. The filter format is
  // documented at
  // http://www.chromium.org/administrators/url-blacklist-filter-format.
  void Block(const base::ListValue* filters);

  // URLs matching one of the |filters| will be allowed. If a URL is both
  // Blocked and Allowed, Allow takes precedence.
  void Allow(const base::ListValue* filters);

  // Returns true if the URL is blocked.
  bool IsURLBlocked(const GURL& url) const;

  // Returns true if the URL has a standard scheme. Only URLs with standard
  // schemes are filtered.
  static bool HasStandardScheme(const GURL& url);

  // Splits a URL filter into its components. A GURL isn't used because these
  // can be invalid URLs e.g. "google.com".
  // Returns false if the URL couldn't be parsed.
  // The |host| is preprocessed so it can be passed to URLMatcher for the
  // appropriate condition.
  // The optional username and password are ignored.
  // |match_subdomains| specifies whether the filter should include subdomains
  // of the hostname (if it is one.)
  // |port| is 0 if none is explicitly defined.
  // |path| does not include query parameters.
  static bool FilterToComponents(const std::string& filter,
                                 std::string* scheme,
                                 std::string* host,
                                 bool* match_subdomains,
                                 uint16* port,
                                 std::string* path);

  // Creates a condition set that can be used with the |url_matcher|. |id| needs
  // to be a unique number that will be returned by the |url_matcher| if the URL
  // matches that condition set.
  static scoped_refptr<extensions::URLMatcherConditionSet> CreateConditionSet(
      extensions::URLMatcher* url_matcher,
      extensions::URLMatcherConditionSet::ID id,
      const std::string& scheme,
      const std::string& host,
      bool match_subdomains,
      uint16 port,
      const std::string& path);

 private:
  struct FilterComponents;

  // Returns true if |lhs| takes precedence over |rhs|.
  static bool FilterTakesPrecedence(const FilterComponents& lhs,
                                    const FilterComponents& rhs);

  extensions::URLMatcherConditionSet::ID id_;
  std::map<extensions::URLMatcherConditionSet::ID, FilterComponents> filters_;
  scoped_ptr<extensions::URLMatcher> url_matcher_;

  DISALLOW_COPY_AND_ASSIGN(URLBlacklist);
};

// Tracks the blacklist policies for a given profile, and updates it on changes.
//
// This class interacts with both the UI thread, where notifications of pref
// changes are received from, and the IO thread, which owns it (in the
// ProfileIOData) and checks for blacklisted URLs (from ChromeNetworkDelegate).
//
// It must be constructed on the UI thread, to set up |ui_weak_ptr_factory_| and
// the prefs listeners.
//
// ShutdownOnUIThread must be called from UI before destruction, to release
// the prefs listeners on the UI thread. This is done from ProfileIOData.
//
// Update tasks from the UI thread can post safely to the IO thread, since the
// destruction order of Profile and ProfileIOData guarantees that if this
// exists in UI, then a potential destruction on IO will come after any task
// posted to IO from that method on UI. This is used to go through IO before
// the actual update starts, and grab a WeakPtr.
class URLBlacklistManager {
 public:
  // Must be constructed on the UI thread.
  explicit URLBlacklistManager(PrefService* pref_service);
  virtual ~URLBlacklistManager();

  // Must be called on the UI thread, before destruction.
  void ShutdownOnUIThread();

  // Returns true if |url| is blocked by the current blacklist. Must be called
  // from the IO thread.
  bool IsURLBlocked(const GURL& url) const;

  // Replaces the current blacklist. Must be called on the IO thread.
  // Virtual for testing.
  virtual void SetBlacklist(scoped_ptr<URLBlacklist> blacklist);

  // Registers the preferences related to blacklisting in the given PrefService.
  static void RegisterUserPrefs(PrefServiceSyncable* pref_service);

 protected:
  // Used to delay updating the blacklist while the preferences are
  // changing, and execute only one update per simultaneous prefs changes.
  void ScheduleUpdate();

  // Updates the blacklist using the current preference values.
  // Virtual for testing.
  virtual void Update();

  // Starts the blacklist update on the IO thread, using the filters in
  // |block| and |allow|. Protected for testing.
  void UpdateOnIO(scoped_ptr<base::ListValue> block,
                  scoped_ptr<base::ListValue> allow);

 private:
  // ---------
  // UI thread
  // ---------

  // Used to post update tasks to the UI thread.
  base::WeakPtrFactory<URLBlacklistManager> ui_weak_ptr_factory_;

  // Used to track the policies and update the blacklist on changes.
  PrefChangeRegistrar pref_change_registrar_;
  PrefService* pref_service_;  // Weak.

  // ---------
  // IO thread
  // ---------

  // Used to get |weak_ptr_| to self on the IO thread.
  base::WeakPtrFactory<URLBlacklistManager> io_weak_ptr_factory_;

  // The current blacklist.
  scoped_ptr<URLBlacklist> blacklist_;

  DISALLOW_COPY_AND_ASSIGN(URLBlacklistManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_URL_BLACKLIST_MANAGER_H_
