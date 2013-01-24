// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_URL_FILTER_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_URL_FILTER_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_mode_site_list.h"
#include "chrome/browser/policy/url_blacklist_manager.h"

namespace policy {
class URLBlacklist;
}  // namespace policy

class FilePath;
class GURL;

// This class manages the filtering behavior for a given URL, i.e. it tells
// callers if a given URL should be allowed, blocked or warned about.
// References to it can be passed around on different threads (the refcounting
// is thread-safe), but the object itself should always be accessed on the same
// thread (member access isn't thread-safe).
class ManagedModeURLFilter
    : public base::RefCountedThreadSafe<ManagedModeURLFilter>,
      public base::NonThreadSafe {
 public:
  enum FilteringBehavior {
    ALLOW,
    WARN,
    BLOCK
  };

  class Observer {
   public:
    virtual void OnSiteListUpdated() = 0;
  };

  struct Contents;

  ManagedModeURLFilter();

  static FilteringBehavior BehaviorFromInt(int behavior_value);

  void GetSites(const GURL& url,
                std::vector<ManagedModeSiteList::Site*>* sites) const;

  // Returns the filtering behavior for a given URL, based on the default
  // behavior and whether it is on a site list.
  FilteringBehavior GetFilteringBehaviorForURL(const GURL& url) const;

  // Sets the filtering behavior for pages not on a list (default is ALLOW).
  void SetDefaultFilteringBehavior(FilteringBehavior behavior);

  // Asynchronously loads the specified site lists from disk and updates the
  // filter to recognize each site on them.
  // Calls |continuation| when the filter has been updated.
  void LoadWhitelists(ScopedVector<ManagedModeSiteList> site_lists);

  // Set the list of matched patterns to the passed in list.
  // This method is only used for testing.
  void SetFromPatterns(const std::vector<std::string>& patterns);

  // Sets the manual lists.
  void SetManualLists(const ListValue* whitelist,
                      const ListValue* blacklist);

  // Adds a pattern to a manual list. If |is_whitelist| is true it gets added
  // to the whitelist, else to the blacklist.
  void AddURLPatternToManualList(const bool is_whitelist,
                                 const std::string& url_pattern);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class base::RefCountedThreadSafe<ManagedModeURLFilter>;
  ~ManagedModeURLFilter();

  void SetContents(scoped_ptr<Contents> url_matcher);

  ObserverList<Observer> observers_;

  FilteringBehavior default_behavior_;
  scoped_ptr<Contents> contents_;

  // The |url_manual_list_allow_| blocks all URLs except the ones that are
  // added while the |url_manual_list_block_| blocks only the URLs that are
  // added to it.
  scoped_ptr<policy::URLBlacklist> url_manual_list_allow_;
  scoped_ptr<policy::URLBlacklist> url_manual_list_block_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeURLFilter);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_URL_FILTER_H_
