// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_set.h"

#include "base/logging.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handlers/sandboxed_page_info.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"

using WebKit::WebSecurityOrigin;
using extensions::Extension;

ExtensionURLInfo::ExtensionURLInfo(WebSecurityOrigin origin, const GURL& url)
  : origin_(origin),
    url_(url) {
  DCHECK(!origin_.isNull());
}

ExtensionURLInfo::ExtensionURLInfo(const GURL& url)
  : url_(url) {
}

ExtensionSet::const_iterator::const_iterator() {}

ExtensionSet::const_iterator::const_iterator(const const_iterator& other)
    : it_(other.it_) {
}

ExtensionSet::const_iterator::const_iterator(ExtensionMap::const_iterator it)
    : it_(it) {
}

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

void ExtensionSet::Insert(const scoped_refptr<const Extension>& extension) {
  extensions_[extension->id()] = extension;
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
  return extensions_.erase(id) > 0;
}

void ExtensionSet::Clear() {
  extensions_.clear();
}

std::string ExtensionSet::GetExtensionOrAppIDByURL(
    const ExtensionURLInfo& info) const {
  DCHECK(!info.origin().isNull());

  if (info.url().SchemeIs(extensions::kExtensionScheme))
    return info.origin().isUnique() ? "" : info.url().host();

  const Extension* extension = GetExtensionOrAppByURL(info);
  if (!extension)
    return "";

  return extension->id();
}

const Extension* ExtensionSet::GetExtensionOrAppByURL(
    const ExtensionURLInfo& info) const {
  // In the common case, the document's origin will correspond to its URL,
  // but in some rare cases involving sandboxing, the two will be different.
  // We catch those cases by checking whether the document's origin is unique.
  // If that's not the case, then we conclude that the document's security
  // context is well-described by its URL and proceed to use only the URL.
  if (!info.origin().isNull() && info.origin().isUnique())
    return NULL;

  if (info.url().SchemeIs(extensions::kExtensionScheme))
    return GetByID(info.url().host());

  return GetHostedAppByURL(info);
}

const Extension* ExtensionSet::GetHostedAppByURL(
    const ExtensionURLInfo& info) const {
  for (ExtensionMap::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    if (iter->second->web_extent().MatchesURL(info.url()))
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
  return GetExtensionOrAppByURL(ExtensionURLInfo(old_url)) ==
      GetExtensionOrAppByURL(ExtensionURLInfo(new_url));
}

const Extension* ExtensionSet::GetByID(const std::string& id) const {
  ExtensionMap::const_iterator i = extensions_.find(id);
  if (i != extensions_.end())
    return i->second.get();
  else
    return NULL;
}

std::set<std::string> ExtensionSet::GetIDs() const {
  std::set<std::string> ids;
  for (ExtensionMap::const_iterator it = extensions_.begin();
       it != extensions_.end(); ++it) {
    ids.insert(it->first);
  }
  return ids;
}

bool ExtensionSet::ExtensionBindingsAllowed(
    const ExtensionURLInfo& info) const {
  if (info.origin().isUnique() || IsSandboxedPage(info))
    return false;

  if (info.url().SchemeIs(extensions::kExtensionScheme))
    return true;

  ExtensionMap::const_iterator i = extensions_.begin();
  for (; i != extensions_.end(); ++i) {
    if (i->second->location() == extensions::Manifest::COMPONENT &&
        i->second->web_extent().MatchesURL(info.url()))
      return true;
  }

  return false;
}

bool ExtensionSet::IsSandboxedPage(const ExtensionURLInfo& info) const {
  if (info.url().SchemeIs(extensions::kExtensionScheme)) {
    const Extension* extension = GetByID(info.url().host());
    if (extension) {
      return extensions::SandboxedPageInfo::IsSandboxedPage(extension,
                                                            info.url().path());
    }
  }
  return false;
}
