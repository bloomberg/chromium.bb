// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_host_to_urls_map.h"

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"

// Indicates if the two inputs have the same security origin.
// |requested_origin| should only be a security origin (no path, etc.).
// It is ok if |template_url| is NULL.
static bool IsSameOrigin(const GURL& requested_origin,
                         const TemplateURL* template_url) {
  // TODO(levin): This isn't safe for the I/O thread yet.
  DCHECK(!ChromeThread::IsWellKnownThread(ChromeThread::UI) ||
         ChromeThread::CurrentlyOn(ChromeThread::UI));

  DCHECK(requested_origin == requested_origin.GetOrigin());
  return template_url && requested_origin ==
      TemplateURLModel::GenerateSearchURL(template_url).GetOrigin();
}

SearchHostToURLsMap::SearchHostToURLsMap()
    : initialized_(false) {
}

void SearchHostToURLsMap::Init(
    const std::vector<const TemplateURL*>& template_urls,
    const TemplateURL* default_search_provider) {
  DCHECK(CalledOnValidThread());
  DCHECK(!initialized_);
  {
    AutoLock locker(lock_);
    for (size_t i = 0; i < template_urls.size(); ++i) {
      AddNoLock(template_urls[i]);
    }
    SetDefaultNoLock(default_search_provider);
  }
  // Wait to set initialized until the lock is let go, which should force a
  // memory write flush.
  initialized_ = true;
}

void SearchHostToURLsMap::Add(const TemplateURL* template_url) {
  DCHECK(CalledOnValidThread());
  DCHECK(initialized_);
  DCHECK(template_url);

  AutoLock locker(lock_);
  AddNoLock(template_url);
}

void SearchHostToURLsMap::Remove(const TemplateURL* template_url) {
  DCHECK(CalledOnValidThread());
  DCHECK(initialized_);
  DCHECK(template_url);

  AutoLock locker(lock_);
  RemoveNoLock(template_url);
}

void SearchHostToURLsMap::Update(const TemplateURL* existing_turl,
                                 const TemplateURL& new_values) {
  DCHECK(CalledOnValidThread());
  DCHECK(initialized_);
  DCHECK(existing_turl);

  AutoLock locker(lock_);

  RemoveNoLock(existing_turl);

  // Use the information from new_values but preserve existing_turl's id.
  TemplateURLID previous_id = existing_turl->id();
  TemplateURL* modifiable_turl = const_cast<TemplateURL*>(existing_turl);
  *modifiable_turl = new_values;
  modifiable_turl->set_id(previous_id);

  AddNoLock(existing_turl);
}

void SearchHostToURLsMap::RemoveAll() {
  DCHECK(CalledOnValidThread());

  AutoLock locker(lock_);
  host_to_urls_map_.clear();
}

void SearchHostToURLsMap::UpdateGoogleBaseURLs() {
  DCHECK(CalledOnValidThread());
  DCHECK(initialized_);

  AutoLock locker(lock_);

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
    RemoveByPointerNoLock(t_urls_using_base_url[i]);

  for (size_t i = 0; i < t_urls_using_base_url.size(); ++i)
    AddNoLock(t_urls_using_base_url[i]);
}

void SearchHostToURLsMap::SetDefault(const TemplateURL* template_url) {
  DCHECK(CalledOnValidThread());
  DCHECK(initialized_);

  AutoLock locker(lock_);
  SetDefaultNoLock(template_url);
}

const TemplateURL* SearchHostToURLsMap::GetTemplateURLForHost(
    const std::string& host) const {
  DCHECK(CalledOnValidThread());
  DCHECK(initialized_);

  HostToURLsMap::const_iterator iter = host_to_urls_map_.find(host);
  if (iter == host_to_urls_map_.end() || iter->second.empty())
    return NULL;
  return *(iter->second.begin());  // Return the 1st element.
}

const SearchHostToURLsMap::TemplateURLSet* SearchHostToURLsMap::GetURLsForHost(
    const std::string& host) const {
  DCHECK(CalledOnValidThread());
  DCHECK(initialized_);

  HostToURLsMap::const_iterator urls_for_host = host_to_urls_map_.find(host);
  if (urls_for_host == host_to_urls_map_.end() || urls_for_host->second.empty())
    return NULL;
  return &urls_for_host->second;
}

SearchProviderInstallData::State SearchHostToURLsMap::GetInstallState(
    const GURL& requested_origin) {
  AutoLock locker(lock_);
  if (!initialized_)
    return NOT_READY;

  // First check to see if the origin is the default search provider.
  if (requested_origin.spec() == default_search_origin_)
    return INSTALLED_AS_DEFAULT;

  // Is the url any search provider?
  HostToURLsMap::const_iterator urls_for_host =
      host_to_urls_map_.find(requested_origin.host());
  if (urls_for_host == host_to_urls_map_.end() ||
      urls_for_host->second.empty()) {
    return NOT_INSTALLED;
  }

  const TemplateURLSet& urls = urls_for_host->second;
  for (TemplateURLSet::const_iterator i = urls.begin();
       i != urls.end(); ++i) {
    const TemplateURL* template_url = *i;
    if (IsSameOrigin(requested_origin, template_url))
      return INSTALLED_BUT_NOT_DEFAULT;
  }
  return NOT_INSTALLED;
}

SearchHostToURLsMap::~SearchHostToURLsMap() {
}

void SearchHostToURLsMap::AddNoLock(const TemplateURL* template_url) {
  DCHECK(CalledOnValidThread());

  lock_.AssertAcquired();
  const GURL url(TemplateURLModel::GenerateSearchURL(template_url));
  if (!url.is_valid() || !url.has_host())
    return;

  host_to_urls_map_[url.host()].insert(template_url);
}

void SearchHostToURLsMap::RemoveNoLock(const TemplateURL* template_url) {
  DCHECK(CalledOnValidThread());
  lock_.AssertAcquired();

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

void SearchHostToURLsMap::RemoveByPointerNoLock(
    const TemplateURL* template_url) {
  DCHECK(CalledOnValidThread());
  lock_.AssertAcquired();

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

void SearchHostToURLsMap::SetDefaultNoLock(const TemplateURL* template_url) {
  DCHECK(CalledOnValidThread());
  lock_.AssertAcquired();

  if (!template_url) {
    default_search_origin_.clear();
    return;
  }

  const GURL url(TemplateURLModel::GenerateSearchURL(template_url));
  if (!url.is_valid() || !url.has_host()) {
    default_search_origin_.clear();
    return;
  }
  default_search_origin_ = url.GetOrigin().spec();
}
