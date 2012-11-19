// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_URL_FILTER_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_URL_FILTER_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_mode_site_list.h"

class FilePath;
class GURL;

// This class manages the filtering behavior for a given URL, i.e. it tells
// callers if a given URL should be allowed, blocked or warned about.
// It is not thread-safe, so it can only be used on one thread, but that can be
// any thread.
class ManagedModeURLFilter : public base::NonThreadSafe {
 public:
  enum FilteringBehavior {
    ALLOW,
    WARN,
    BLOCK
  };

  struct Contents;

  ManagedModeURLFilter();
  ~ManagedModeURLFilter();

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
  void LoadWhitelists(ScopedVector<ManagedModeSiteList> site_lists,
                      const base::Closure& continuation);

  // Set the list of matched patterns to the passed in list.
  // This method is only used for testing.
  void SetFromPatterns(const std::vector<std::string>& patterns,
                       const base::Closure& continuation);

 private:
  void SetContents(const base::Closure& callback,
                   scoped_ptr<Contents> url_matcher);

  base::WeakPtrFactory<ManagedModeURLFilter> weak_ptr_factory_;
  FilteringBehavior default_behavior_;
  scoped_ptr<Contents> contents_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeURLFilter);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_URL_FILTER_H_
