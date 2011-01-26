// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_renderer_info.h"

#include "base/logging.h"
#include "chrome/common/url_constants.h"

ExtensionRendererInfo::ExtensionRendererInfo() {
}

size_t ExtensionRendererInfo::size() const {
  return extensions_.size();
}

void ExtensionRendererInfo::Update(
    const scoped_refptr<const Extension>& extension) {
  extensions_[extension->id()] = extension;
}

void ExtensionRendererInfo::Remove(const std::string& id) {
  extensions_.erase(id);
}

std::string ExtensionRendererInfo::GetIdByURL(const GURL& url) const {
  if (url.SchemeIs(chrome::kExtensionScheme))
    return url.host();

  const Extension* extension = GetByURL(url);
  if (!extension)
    return "";

  return extension->id();
}

const Extension* ExtensionRendererInfo::GetByURL(
    const GURL& url) const {
  if (url.SchemeIs(chrome::kExtensionScheme))
    return GetByID(url.host());

  ExtensionMap::const_iterator i = extensions_.begin();
  for (; i != extensions_.end(); ++i) {
    if (i->second->web_extent().ContainsURL(url))
      return i->second.get();
  }

  return NULL;
}

bool ExtensionRendererInfo::InSameExtent(const GURL& old_url,
                                         const GURL& new_url) const {
  return GetByURL(old_url) == GetByURL(new_url);
}

const Extension* ExtensionRendererInfo::GetByID(
    const std::string& id) const {
  ExtensionMap::const_iterator i = extensions_.find(id);
  if (i != extensions_.end())
    return i->second.get();
  else
    return NULL;
}

bool ExtensionRendererInfo::ExtensionBindingsAllowed(const GURL& url) const {
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
