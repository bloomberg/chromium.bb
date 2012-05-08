// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_host_to_urls_map.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"

SearchHostToURLsMap::SearchHostToURLsMap()
    : initialized_(false) {
}

SearchHostToURLsMap::~SearchHostToURLsMap() {
}

void SearchHostToURLsMap::Init(
    const TemplateURLService::TemplateURLVector& template_urls,
    const SearchTermsData& search_terms_data) {
  DCHECK(!initialized_);
  initialized_ = true;  // Set here so Add doesn't assert.
  Add(template_urls, search_terms_data);
}

void SearchHostToURLsMap::Add(TemplateURL* template_url,
                              const SearchTermsData& search_terms_data) {
  DCHECK(initialized_);
  DCHECK(template_url);
  DCHECK(!template_url->IsExtensionKeyword());

  const GURL url(TemplateURLService::GenerateSearchURLUsingTermsData(
      template_url, search_terms_data));
  if (!url.is_valid() || !url.has_host())
    return;

  host_to_urls_map_[url.host()].insert(template_url);
}

void SearchHostToURLsMap::Remove(TemplateURL* template_url) {
  DCHECK(initialized_);
  DCHECK(template_url);
  DCHECK(!template_url->IsExtensionKeyword());

  const GURL url(TemplateURLService::GenerateSearchURL(template_url));
  if (!url.is_valid() || !url.has_host())
    return;

  const std::string host(url.host());
  DCHECK(host_to_urls_map_.find(host) != host_to_urls_map_.end());

  TemplateURLSet& urls = host_to_urls_map_[host];
  DCHECK(urls.find(template_url) != urls.end());

  urls.erase(urls.find(template_url));
  if (urls.empty())
    host_to_urls_map_.erase(host_to_urls_map_.find(host));
}

void SearchHostToURLsMap::UpdateGoogleBaseURLs(
    const SearchTermsData& search_terms_data) {
  DCHECK(initialized_);

  // Create a list of the the TemplateURLs to update.
  TemplateURLService::TemplateURLVector t_urls_using_base_url;
  for (HostToURLsMap::iterator i(host_to_urls_map_.begin());
       i != host_to_urls_map_.end(); ++i) {
    for (TemplateURLSet::const_iterator j(i->second.begin());
         j != i->second.end(); ++j) {
      if ((*j)->url_ref().HasGoogleBaseURLs() ||
          (*j)->suggestions_url_ref().HasGoogleBaseURLs())
        t_urls_using_base_url.push_back(*j);
    }
  }

  for (TemplateURLService::TemplateURLVector::const_iterator i(
       t_urls_using_base_url.begin()); i != t_urls_using_base_url.end(); ++i)
    RemoveByPointer(*i);

  Add(t_urls_using_base_url, search_terms_data);
}

TemplateURL* SearchHostToURLsMap::GetTemplateURLForHost(
    const std::string& host) {
  DCHECK(initialized_);

  HostToURLsMap::const_iterator iter = host_to_urls_map_.find(host);
  if (iter == host_to_urls_map_.end() || iter->second.empty())
    return NULL;
  return *(iter->second.begin());  // Return the 1st element.
}

SearchHostToURLsMap::TemplateURLSet* SearchHostToURLsMap::GetURLsForHost(
    const std::string& host) {
  DCHECK(initialized_);

  HostToURLsMap::iterator urls_for_host = host_to_urls_map_.find(host);
  if (urls_for_host == host_to_urls_map_.end() || urls_for_host->second.empty())
    return NULL;
  return &urls_for_host->second;
}

void SearchHostToURLsMap::Add(
    const TemplateURLService::TemplateURLVector& template_urls,
    const SearchTermsData& search_terms_data) {
  for (TemplateURLService::TemplateURLVector::const_iterator i(
       template_urls.begin()); i != template_urls.end(); ++i)
    Add(*i, search_terms_data);
}

void SearchHostToURLsMap::RemoveByPointer(TemplateURL* template_url) {
  for (HostToURLsMap::iterator i = host_to_urls_map_.begin();
       i != host_to_urls_map_.end(); ++i) {
    TemplateURLSet::iterator url_set_iterator = i->second.find(template_url);
    if (url_set_iterator != i->second.end()) {
      i->second.erase(url_set_iterator);
      if (i->second.empty())
        host_to_urls_map_.erase(i);
      // A given TemplateURL only occurs once in the map. As soon as we find the
      // entry, stop.
      return;
    }
  }
}
