// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_SUGGESTED_LINKS_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_SUGGESTED_LINKS_REGISTRY_H_
#pragma once

#include <map>

#include "chrome/browser/extensions/api/discovery/suggested_link.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

// This class keeps track of links suggested by an extension using the discovery
// API.
class SuggestedLinksRegistry : public ProfileKeyedService {
 public:
  // A list of ExtensionSuggestedLink's.
  typedef std::vector<linked_ptr<extensions::SuggestedLink> > SuggestedLinkList;

  SuggestedLinksRegistry();

  // Adds a suggested link from |extension_id|. Takes ownership of |item| in all
  // cases.
  void Add(const std::string& extension_id,
      scoped_ptr<extensions::SuggestedLink> item);

  // Returns all the links suggested by |extension_id|.
  const SuggestedLinkList* GetAll(const std::string& extension_id) const;

  // Remove a specific link suggested by |extension_id|.
  void Remove(const std::string& extension_id, const std::string& link_url);

  // Clears all suggested links for |extension_id|.
  void ClearAll(const std::string& extension_id);

 private:
  // Maps extension id to a list of notifications for that extension.
  typedef std::map<std::string, SuggestedLinkList> SuggestedLinksMap;

  virtual ~SuggestedLinksRegistry();

  // Gets suggested links for a given extension id.
  SuggestedLinkList& GetAllInternal(const std::string& extension_id);

  SuggestedLinksMap suggested_links_;

  DISALLOW_COPY_AND_ASSIGN(SuggestedLinksRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_SUGGESTED_LINKS_REGISTRY_H_
