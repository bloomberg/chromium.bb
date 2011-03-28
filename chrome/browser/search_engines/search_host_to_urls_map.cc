// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_host_to_urls_map.h"

#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"

SearchHostToURLsMap::SearchHostToURLsMap()
    : initialized_(false) {
}

SearchHostToURLsMap::~SearchHostToURLsMap() {
}

void SearchHostToURLsMap::Init(
    const std::vector<const TemplateURL*>& template_urls,
    const SearchTermsData& search_terms_data) {
  DCHECK(!initialized_);

  // Set as initialized here so Add doesn't assert.
  initialized_ = true;

  for (size_t i = 0; i < template_urls.size(); ++i)
    Add(template_urls[i], search_terms_data);
}

void SearchHostToURLsMap::Add(const TemplateURL* template_url,
                              const SearchTermsData& search_terms_data) {
  DCHECK(initialized_);
  DCHECK(template_url);

  const GURL url(TemplateURLModel::GenerateSearchURLUsingTermsData(
      template_url, search_terms_data));
  if (!url.is_valid() || !url.has_host())
    return;

  host_to_urls_map_[url.host()].insert(template_url);
}

void SearchHostToURLsMap::Remove(const TemplateURL* template_url) {
  DCHECK(initialized_);
  DCHECK(template_url);

  const GURL url(TemplateURLModel::GenerateSearchURL(template_url));
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

void SearchHostToURLsMap::Update(const TemplateURL* existing_turl,
                                 const TemplateURL& new_values,
                                 const SearchTermsData& search_terms_data) {
  DCHECK(initialized_);
  DCHECK(existing_turl);

  Remove(existing_turl);

  // Use the information from new_values but preserve existing_turl's id.
  TemplateURLID previous_id = existing_turl->id();
  TemplateURL* modifiable_turl = const_cast<TemplateURL*>(existing_turl);
  *modifiable_turl = new_values;
  modifiable_turl->set_id(previous_id);

  Add(existing_turl, search_terms_data);
}

void SearchHostToURLsMap::UpdateGoogleBaseURLs(
    const SearchTermsData& search_terms_data) {
  DCHECK(initialized_);

  // Create a list of the the TemplateURLs to update.
  std::vector<const TemplateURL*> t_urls_using_base_url;
  for (HostToURLsMap::iterator host_map_iterator = host_to_urls_map_.begin();
       host_map_iterator != host_to_urls_map_.end(); ++host_map_iterator) {
    const TemplateURLSet& urls = host_map_iterator->second;
    for (TemplateURLSet::const_iterator url_set_iterator = urls.begin();
         url_set_iterator != urls.end(); ++url_set_iterator) {
      const TemplateURL* t_url = *url_set_iterator;
      if ((t_url->url() && t_url->url()->HasGoogleBaseURLs()) ||
          (t_url->suggestions_url() &&
           t_url->suggestions_url()->HasGoogleBaseURLs())) {
        t_urls_using_base_url.push_back(t_url);
      }
    }
  }

  for (size_t i = 0; i < t_urls_using_base_url.size(); ++i)
    RemoveByPointer(t_urls_using_base_url[i]);

  for (size_t i = 0; i < t_urls_using_base_url.size(); ++i)
    Add(t_urls_using_base_url[i], search_terms_data);
}

const TemplateURL* SearchHostToURLsMap::GetTemplateURLForHost(
    const std::string& host) const {
  DCHECK(initialized_);

  HostToURLsMap::const_iterator iter = host_to_urls_map_.find(host);
  if (iter == host_to_urls_map_.end() || iter->second.empty())
    return NULL;
  return *(iter->second.begin());  // Return the 1st element.
}

const SearchHostToURLsMap::TemplateURLSet* SearchHostToURLsMap::GetURLsForHost(
    const std::string& host) const {
  DCHECK(initialized_);

  HostToURLsMap::const_iterator urls_for_host = host_to_urls_map_.find(host);
  if (urls_for_host == host_to_urls_map_.end() || urls_for_host->second.empty())
    return NULL;
  return &urls_for_host->second;
}

void SearchHostToURLsMap::RemoveByPointer(
    const TemplateURL* template_url) {
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
