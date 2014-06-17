// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTER_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/browser/supervised_user/supervised_users.h"

class GURL;

// This class manages the filtering behavior for a given URL, i.e. it tells
// callers if a given URL should be allowed, blocked or warned about. It uses
// information from multiple sources:
//   * A default setting (allow, block or warn).
//   * The set of installed and enabled content packs, which contain whitelists
//     of URL patterns that should be allowed.
//   * User-specified manual overrides (allow or block) for either sites
//     (hostnames) or exact URLs, which take precedence over the previous
//     sources.
// References to it can be passed around on different threads (the refcounting
// is thread-safe), but the object itself should always be accessed on the same
// thread (member access isn't thread-safe).
class SupervisedUserURLFilter
    : public base::RefCountedThreadSafe<SupervisedUserURLFilter>,
      public base::NonThreadSafe {
 public:
  enum FilteringBehavior {
    ALLOW,
    WARN,
    BLOCK,
    HISTOGRAM_BOUNDING_VALUE
  };

  class Observer {
   public:
    virtual void OnSiteListUpdated() = 0;
  };

  struct Contents;

  SupervisedUserURLFilter();

  static FilteringBehavior BehaviorFromInt(int behavior_value);

  // Normalizes a URL for matching purposes.
  static GURL Normalize(const GURL& url);

  // Returns true if the URL has a standard scheme. Only URLs with standard
  // schemes are filtered.
  // This method is public for testing.
  static bool HasFilteredScheme(const GURL& url);

  // Returns true if the |host| matches the pattern. A pattern is a hostname
  // with one or both of the following modifications:
  // - If the pattern starts with "*.", it matches the host or any subdomain
  //   (e.g. the pattern "*.google.com" would match google.com, www.google.com,
  //   or accounts.google.com).
  // - If the pattern ends with ".*", it matches the host on any known TLD
  //   (e.g. the pattern "google.*" would match google.com or google.co.uk).
  // See the SupervisedUserURLFilterTest.HostMatchesPattern unit test for more
  // examples.
  // Asterisks in other parts of the pattern are not allowed.
  // |host| and |pattern| are assumed to be normalized to lower-case.
  // This method is public for testing.
  static bool HostMatchesPattern(const std::string& host,
                                 const std::string& pattern);

  void GetSites(const GURL& url,
                std::vector<SupervisedUserSiteList::Site*>* sites) const;

  // Returns the filtering behavior for a given URL, based on the default
  // behavior and whether it is on a site list.
  FilteringBehavior GetFilteringBehaviorForURL(const GURL& url) const;

  // Sets the filtering behavior for pages not on a list (default is ALLOW).
  void SetDefaultFilteringBehavior(FilteringBehavior behavior);

  // Asynchronously loads the specified site lists from disk and updates the
  // filter to recognize each site on them.
  // Calls |continuation| when the filter has been updated.
  void LoadWhitelists(ScopedVector<SupervisedUserSiteList> site_lists);

  // Set the list of matched patterns to the passed in list.
  // This method is only used for testing.
  void SetFromPatterns(const std::vector<std::string>& patterns);

  // Sets the set of manually allowed or blocked hosts.
  void SetManualHosts(const std::map<std::string, bool>* host_map);

  // Sets the set of manually allowed or blocked URLs.
  void SetManualURLs(const std::map<GURL, bool>* url_map);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class base::RefCountedThreadSafe<SupervisedUserURLFilter>;
  ~SupervisedUserURLFilter();

  void SetContents(scoped_ptr<Contents> url_matcher);

  ObserverList<Observer> observers_;

  FilteringBehavior default_behavior_;
  scoped_ptr<Contents> contents_;

  // Maps from a URL to whether it is manually allowed (true) or blocked
  // (false).
  std::map<GURL, bool> url_map_;

  // Maps from a hostname to whether it is manually allowed (true) or blocked
  // (false).
  std::map<std::string, bool> host_map_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserURLFilter);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTER_H_
