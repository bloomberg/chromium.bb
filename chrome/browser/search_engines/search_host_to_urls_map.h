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

class SearchTermsData;
class TemplateURL;

// Holds the host to template url mappings for the search providers. WARNING:
// This class does not own any TemplateURLs passed to it and it is up to the
// caller to ensure the right lifetime of them.
class SearchHostToURLsMap {
 public:
  typedef std::set<const TemplateURL*> TemplateURLSet;

  SearchHostToURLsMap();
  ~SearchHostToURLsMap();

  // Initializes the map.
  void Init(const std::vector<const TemplateURL*>& template_urls,
            const SearchTermsData& search_terms_data);

  // Adds a new TemplateURL to the map. Since |template_url| is owned
  // externally, Remove or RemoveAll should be called if it becomes invalid.
  void Add(const TemplateURL* template_url,
           const SearchTermsData& search_terms_data);

  // Removes the TemplateURL from the lookup.
  void Remove(const TemplateURL* template_url);

  // Updates information in an existing TemplateURL. Note: Using Remove and
  // then Add separately would lead to race conditions in reading because the
  // lock would be released inbetween the calls.
  void Update(const TemplateURL* existing_turl,
              const TemplateURL& new_values,
              const SearchTermsData& search_terms_data);

  // Updates all search providers which have a google base url.
  void UpdateGoogleBaseURLs(const SearchTermsData& search_terms_data);

  // Returns the first TemplateURL found with a URL using the specified |host|,
  // or NULL if there are no such TemplateURLs
  const TemplateURL* GetTemplateURLForHost(const std::string& host) const;

  // Return the TemplateURLSet for the given the |host| or NULL if there are
  // none.
  const TemplateURLSet* GetURLsForHost(const std::string& host) const;

 private:
  friend class SearchHostToURLsMapTest;

  typedef std::map<std::string, TemplateURLSet> HostToURLsMap;

  // Removes the given template_url using only the pointer instead of the value.
  // This is useful when the value may have changed before being updated in the
  // map. (Specifically when the GoogleBaseURLValue changes.)
  void RemoveByPointer(const TemplateURL* template_url);

  // Maps from host to set of TemplateURLs whose search url host is host.
  HostToURLsMap host_to_urls_map_;

  // The security origin for the default search provider.
  std::string default_search_origin_;

  // Has Init been called?
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(SearchHostToURLsMap);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_HOST_TO_URLS_MAP_H_
