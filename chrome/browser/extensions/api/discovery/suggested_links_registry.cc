// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/discovery/suggested_links_registry.h"

namespace {

typedef extensions::SuggestedLinksRegistry::SuggestedLinkList SuggestedLinkList;

SuggestedLinkList::iterator FindUrlInList(const std::string& link_url,
                                          SuggestedLinkList* list) {
  SuggestedLinkList::iterator found = list->begin();
  for (; found != list->end(); ++found)
    if (link_url.compare((*found)->link_url()) == 0) break;
  return found;
}

void RemoveLinkFromList(const std::string& link_url, SuggestedLinkList* list) {
  SuggestedLinkList::iterator found = FindUrlInList(link_url, list);
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
  SuggestedLinkList::iterator found = FindUrlInList(item->link_url(), &list);
  linked_ptr<extensions::SuggestedLink> new_item(item.release());
  if (found != list.end())
    *found = new_item;
  else
    list.push_back(new_item);
}

scoped_ptr<std::vector<std::string> >
    SuggestedLinksRegistry::GetExtensionIds() const {
  scoped_ptr<std::vector<std::string> > result(new std::vector<std::string>());
  for (SuggestedLinksMap::const_iterator it = suggested_links_.begin();
       it != suggested_links_.end(); ++it) {
    result->push_back((*it).first);
  }
  return result.Pass();
}

const SuggestedLinkList* SuggestedLinksRegistry::GetAll(
    const std::string& extension_id) const {
  SuggestedLinksMap::const_iterator found = suggested_links_.find(extension_id);
  if (found != suggested_links_.end())
    return &found->second;
  return &empty_list_;
}

void SuggestedLinksRegistry::Remove(const std::string& extension_id,
    const std::string& link_url) {
  SuggestedLinksMap::iterator found = suggested_links_.find(extension_id);
  if (found != suggested_links_.end()) {
    RemoveLinkFromList(link_url, &found->second);
    if (found->second.empty()) {
      suggested_links_.erase(found);
    }
  }
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
