// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_URL_BLACKLIST_MANAGER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_URL_BLACKLIST_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "components/policy/policy_export.h"
#include "components/url_matcher/url_matcher.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class ListValue;
class SequencedTaskRunner;
}

namespace net {
class URLRequest;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {

// Contains a set of filters to block and allow certain URLs, and matches GURLs
// against this set. The filters are currently kept in memory.
class POLICY_EXPORT URLBlacklist {
 public:
  // This is meant to be bound to url_fixer::SegmentURL. See that function
  // for documentation on the parameters and return value.
  typedef std::string (*SegmentURLCallback)(const std::string&, url::Parsed*);

  explicit URLBlacklist(SegmentURLCallback segment_url);
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

  // Returns the number of items in the list.
  size_t Size() const;

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
  // |query| contains the query parameters ('?' not included).
  static bool FilterToComponents(SegmentURLCallback segment_url,
                                 const std::string& filter,
                                 std::string* scheme,
                                 std::string* host,
                                 bool* match_subdomains,
                                 uint16* port,
                                 std::string* path,
                                 std::string* query);

  // Creates a condition set that can be used with the |url_matcher|. |id| needs
  // to be a unique number that will be returned by the |url_matcher| if the URL
  // matches that condition set. |allow| indicates if it is a white-list (true)
  // or black-list (false) filter.
  static scoped_refptr<url_matcher::URLMatcherConditionSet> CreateConditionSet(
      url_matcher::URLMatcher* url_matcher,
      url_matcher::URLMatcherConditionSet::ID id,
      const std::string& scheme,
      const std::string& host,
      bool match_subdomains,
      uint16 port,
      const std::string& path,
      const std::string& query,
      bool allow);

 private:
  struct FilterComponents;

  // Returns true if |lhs| takes precedence over |rhs|.
  static bool FilterTakesPrecedence(const FilterComponents& lhs,
                                    const FilterComponents& rhs);

  SegmentURLCallback segment_url_;
  url_matcher::URLMatcherConditionSet::ID id_;
  std::map<url_matcher::URLMatcherConditionSet::ID, FilterComponents> filters_;
  scoped_ptr<url_matcher::URLMatcher> url_matcher_;

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
class POLICY_EXPORT URLBlacklistManager {
 public:
  // Returns true if the blacklist should be overridden for |url| and sets
  // |block| to true if it should be blocked and false otherwise.
  // |reason| is set to the exact reason for blocking |url| iff |block| is true.
  typedef base::Callback<bool(const GURL& url, bool* block, int* reason)>
      OverrideBlacklistCallback;

  // Must be constructed on the UI thread.
  // |background_task_runner| is used to build the blacklist in a background
  // thread.
  // |io_task_runner| must be backed by the IO thread.
  // |segment_url| is used to break a URL spec into its components.
  URLBlacklistManager(
      PrefService* pref_service,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
      URLBlacklist::SegmentURLCallback segment_url,
      OverrideBlacklistCallback override_blacklist);
  virtual ~URLBlacklistManager();

  // Must be called on the UI thread, before destruction.
  void ShutdownOnUIThread();

  // Returns true if |url| is blocked by the current blacklist. Must be called
  // from the IO thread.
  bool IsURLBlocked(const GURL& url) const;

  // Returns true if |request| is blocked by the current blacklist.
  // Only main frame and sub frame requests may be blocked; other sub resources
  // or background downloads (e.g. extensions updates, sync, etc) are not
  // filtered. The sync signin page is also not filtered.
  // |reason| is populated with the exact reason for blocking the url if and
  // only if the return value is true otherwise it is left untouched.
  // Must be called from the IO thread.
  bool IsRequestBlocked(const net::URLRequest& request, int* reason) const;

  // Replaces the current blacklist. Must be called on the IO thread.
  // Virtual for testing.
  virtual void SetBlacklist(scoped_ptr<URLBlacklist> blacklist);

  // Registers the preferences related to blacklisting in the given PrefService.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

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

  // Used to post tasks to a background thread.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Used to post tasks to the IO thread.
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // Used to break a URL into its components.
  URLBlacklist::SegmentURLCallback segment_url_;

  // Used to optionally skip blacklisting for some URLs.
  OverrideBlacklistCallback override_blacklist_;

  // ---------
  // IO thread
  // ---------

  // Used to get |weak_ptr_| to self on the IO thread.
  base::WeakPtrFactory<URLBlacklistManager> io_weak_ptr_factory_;

  // Used to post tasks to the UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // The current blacklist.
  scoped_ptr<URLBlacklist> blacklist_;

  DISALLOW_COPY_AND_ASSIGN(URLBlacklistManager);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_URL_BLACKLIST_MANAGER_H_
