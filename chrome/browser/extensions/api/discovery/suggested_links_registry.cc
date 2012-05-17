// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/discovery/suggested_links_registry.h"

namespace {

typedef extensions::SuggestedLinksRegistry::SuggestedLinkList SuggestedLinkList;

void RemoveLinkFromList(const std::string& link_url, SuggestedLinkList* list) {
  SuggestedLinkList::iterator found = list->begin();
  for (; found != list->end(); ++found)
    if (link_url.compare((*found)->link_url()) == 0) break;
  if (found != list->end())
    list->erase(found);
}

}  // namespace

namespace extensions {

SuggestedLinksRegistry::SuggestedLinksRegistry() {}

SuggestedLinksRegistry::~SuggestedLinksRegistry() {
}

void SuggestedLinksRegistry::Add(const std::string& extension_id,
                                 scoped_ptr<extensions::SuggestedLink> item) {
  SuggestedLinkList& list = GetAllInternal(extension_id);
  list.push_back(linked_ptr<extensions::SuggestedLink>(item.release()));
}

const SuggestedLinkList* SuggestedLinksRegistry::GetAll(
    const std::string& extension_id) const {
  SuggestedLinksMap::const_iterator found = suggested_links_.find(extension_id);
  if (found != suggested_links_.end())
    return &found->second;
  return NULL;
}

void SuggestedLinksRegistry::Remove(const std::string& extension_id,
    const std::string& link_url) {
  SuggestedLinksMap::iterator found = suggested_links_.find(extension_id);
  if (found != suggested_links_.end())
    RemoveLinkFromList(link_url, &found->second);
}

void SuggestedLinksRegistry::ClearAll(const std::string& extension_id) {
  SuggestedLinksMap::iterator found = suggested_links_.find(extension_id);
  if (found != suggested_links_.end())
    suggested_links_.erase(found);
}

SuggestedLinkList& SuggestedLinksRegistry::GetAllInternal(
    const std::string& extension_id) {
  // |insert| returns the element if it's already in the map.
  SuggestedLinksMap::iterator found = suggested_links_.insert(
      SuggestedLinksMap::value_type(extension_id, SuggestedLinkList())).first;
  CHECK(found != suggested_links_.end());
  return found->second;
}

}  // namespace extensions
