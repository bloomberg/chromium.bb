// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_set.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/sandboxed_page_info.h"

using extensions::Extension;

ExtensionSet::const_iterator::const_iterator() {}

ExtensionSet::const_iterator::const_iterator(const const_iterator& other)
    : it_(other.it_) {
}

ExtensionSet::const_iterator::const_iterator(ExtensionMap::const_iterator it)
    : it_(it) {
}

ExtensionSet::const_iterator::~const_iterator() {}

ExtensionSet::ExtensionSet() {
}

ExtensionSet::~ExtensionSet() {
}

size_t ExtensionSet::size() const {
  return extensions_.size();
}

bool ExtensionSet::is_empty() const {
  return extensions_.empty();
}

bool ExtensionSet::Contains(const std::string& extension_id) const {
  return extensions_.find(extension_id) != extensions_.end();
}

bool ExtensionSet::Insert(const scoped_refptr<const Extension>& extension) {
  bool was_present = ContainsKey(extensions_, extension->id());
  extensions_[extension->id()] = extension;
  if (!was_present && !modification_callback_.is_null())
    modification_callback_.Run(GetIDs());
  return !was_present;
}

bool ExtensionSet::InsertAll(const ExtensionSet& extensions) {
  size_t before = size();
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    Insert(*iter);
  }
  return size() != before;
}

bool ExtensionSet::Remove(const std::string& id) {
  bool was_present = extensions_.erase(id) > 0;
  if (was_present && !modification_callback_.is_null())
    modification_callback_.Run(GetIDs());
  return was_present;
}

void ExtensionSet::Clear() {
  extensions_.clear();
}

std::string ExtensionSet::GetExtensionOrAppIDByURL(const GURL& url) const {
  if (url.SchemeIs(extensions::kExtensionScheme))
    return url.host();

  const Extension* extension = GetExtensionOrAppByURL(url);
  if (!extension)
    return std::string();

  return extension->id();
}

const Extension* ExtensionSet::GetExtensionOrAppByURL(const GURL& url) const {
  if (url.SchemeIs(extensions::kExtensionScheme))
    return GetByID(url.host());

  return GetHostedAppByURL(url);
}

const Extension* ExtensionSet::GetHostedAppByURL(const GURL& url) const {
  for (ExtensionMap::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    if (iter->second->web_extent().MatchesURL(url))
      return iter->second.get();
  }

  return NULL;
}

const Extension* ExtensionSet::GetHostedAppByOverlappingWebExtent(
    const extensions::URLPatternSet& extent) const {
  for (ExtensionMap::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    if (iter->second->web_extent().OverlapsWith(extent))
      return iter->second.get();
  }

  return NULL;
}

bool ExtensionSet::InSameExtent(const GURL& old_url,
                                const GURL& new_url) const {
  return GetExtensionOrAppByURL(old_url) ==
      GetExtensionOrAppByURL(new_url);
}

const Extension* ExtensionSet::GetByID(const std::string& id) const {
  ExtensionMap::const_iterator i = extensions_.find(id);
  if (i != extensions_.end())
    return i->second.get();
  else
    return NULL;
}

extensions::ExtensionIdSet ExtensionSet::GetIDs() const {
  extensions::ExtensionIdSet ids;
  for (ExtensionMap::const_iterator it = extensions_.begin();
       it != extensions_.end(); ++it) {
    ids.insert(it->first);
  }
  return ids;
}

bool ExtensionSet::ExtensionBindingsAllowed(const GURL& url) const {
  if (url.SchemeIs(extensions::kExtensionScheme))
    return true;

  ExtensionMap::const_iterator i = extensions_.begin();
  for (; i != extensions_.end(); ++i) {
    if (i->second->location() == extensions::Manifest::COMPONENT &&
        i->second->web_extent().MatchesURL(url))
      return true;
  }

  return false;
}
