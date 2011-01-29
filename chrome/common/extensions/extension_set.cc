// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_set.h"

#include "base/logging.h"
#include "chrome/common/url_constants.h"

ExtensionSet::ExtensionSet() {
}

ExtensionSet::~ExtensionSet() {
}

size_t ExtensionSet::size() const {
  return extensions_.size();
}

bool ExtensionSet::Contains(const std::string& extension_id) {
  return extensions_.find(extension_id) != extensions_.end();
}

void ExtensionSet::Insert(const scoped_refptr<const Extension>& extension) {
  extensions_[extension->id()] = extension;
}

void ExtensionSet::Remove(const std::string& id) {
  extensions_.erase(id);
}

std::string ExtensionSet::GetIdByURL(const GURL& url) const {
  if (url.SchemeIs(chrome::kExtensionScheme))
    return url.host();

  const Extension* extension = GetByURL(url);
  if (!extension)
    return "";

  return extension->id();
}

const Extension* ExtensionSet::GetByURL(const GURL& url) const {
  if (url.SchemeIs(chrome::kExtensionScheme))
    return GetByID(url.host());

  ExtensionMap::const_iterator i = extensions_.begin();
  for (; i != extensions_.end(); ++i) {
    if (i->second->web_extent().ContainsURL(url))
      return i->second.get();
  }

  return NULL;
}

bool ExtensionSet::InSameExtent(const GURL& old_url,
                                const GURL& new_url) const {
  return GetByURL(old_url) == GetByURL(new_url);
}

const Extension* ExtensionSet::GetByID(const std::string& id) const {
  ExtensionMap::const_iterator i = extensions_.find(id);
  if (i != extensions_.end())
    return i->second.get();
  else
    return NULL;
}

bool ExtensionSet::ExtensionBindingsAllowed(const GURL& url) const {
  if (url.SchemeIs(chrome::kExtensionScheme))
    return true;

  ExtensionMap::const_iterator i = extensions_.begin();
  for (; i != extensions_.end(); ++i) {
    if (i->second->location() == Extension::COMPONENT &&
        i->second->web_extent().ContainsURL(url))
      return true;
  }

  return false;
}
