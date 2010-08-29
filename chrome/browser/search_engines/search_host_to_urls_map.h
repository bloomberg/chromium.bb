// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_SEARCH_HOST_TO_URLS_MAP_H_
#define CHROME_BROWSER_SEARCH_ENGINES_SEARCH_HOST_TO_URLS_MAP_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/thread_checker.h"
#include "base/ref_counted.h"
#include "chrome/browser/search_engines/search_provider_install_data.h"

class FilePath;
class GURL;
class Task;
class TemplateURL;
class WebDataService;

// Holds the host to template url mappings for the search providers. All public
// methods should happen from the same thread except for GetInstallState which
// may be invoked on any thread. WARNING: This class does not own any
// TemplateURLs passed to it and it is up to the caller to ensure the right
// lifetime of them.
class SearchHostToURLsMap
    : public SearchProviderInstallData,
      public ThreadChecker {
 public:
  typedef std::set<const TemplateURL*> TemplateURLSet;

  SearchHostToURLsMap();

  // Initializes the map.
  void Init(const std::vector<const TemplateURL*>& template_urls,
            const TemplateURL* default_search_provider);

  // Adds a new TemplateURL to the map. Since |template_url| is owned
  // externally, Remove or RemoveAll should be called if it becomes invalid.
  void Add(const TemplateURL* template_url);

  // Removes the TemplateURL from the lookup.
  void Remove(const TemplateURL* template_url);

  // Removes all TemplateURLs.
  void RemoveAll();

  // Updates information in an existing TemplateURL. Note: Using Remove and
  // then Add separately would lead to race conditions in reading because the
  // lock would be released inbetween the calls.
  void Update(const TemplateURL* existing_turl, const TemplateURL& new_values);

  // Updates all search providers which have a google base url.
  void UpdateGoogleBaseURLs();

  // Sets the default search provider. This method doesn't store a reference to
  // the TemplateURL passed in.
  void SetDefault(const TemplateURL* template_url);

  // Returns the first TemplateURL found with a URL using the specified |host|,
  // or NULL if there are no such TemplateURLs
  const TemplateURL* GetTemplateURLForHost(const std::string& host) const;

  // Return the TemplateURLSet for the given the |host| or NULL if there are
  // none.
  const TemplateURLSet* GetURLsForHost(const std::string& host) const;

  // Returns the search provider install state for the given origin.
  // This method may be called on any thread.
  // TODO(levin): Make the above statement true about "any thread".
  virtual State GetInstallState(const GURL& requested_origin);

 private:
  friend class SearchHostToURLsMapTest;

  typedef std::map<std::string, TemplateURLSet> HostToURLsMap;

  virtual ~SearchHostToURLsMap();

  // Same as Add but the lock should already be taken.
  void AddNoLock(const TemplateURL* template_url);

  // Same as Remove but the lock should already be taken.
  void RemoveNoLock(const TemplateURL* template_url);

  // Removes the given template_url using only the pointer instead of the value.
  // This is useful when the value may have changed before being updated in the
  // map. (Specifically when the GoogleBaseURLValue changes.)
  void RemoveByPointerNoLock(const TemplateURL* template_url);

  // Same as SetDefaultNoLock but the lock should already be taken.
  void SetDefaultNoLock(const TemplateURL* template_url);

  // Maps from host to set of TemplateURLs whose search url host is host.
  HostToURLsMap host_to_urls_map_;

  // The security origin for the default search provider.
  std::string default_search_origin_;

  // Has Init been called?
  bool initialized_;

  // Used to guard writes from the UI thread and reads from other threads.
  Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(SearchHostToURLsMap);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_HOST_TO_URLS_MAP_H_
