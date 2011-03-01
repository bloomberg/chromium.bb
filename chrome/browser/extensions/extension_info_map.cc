// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_info_map.h"

#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"

namespace {

static void CheckOnValidThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

}  // namespace

ExtensionInfoMap::ExtensionInfoMap() {
}

ExtensionInfoMap::~ExtensionInfoMap() {
}

void ExtensionInfoMap::AddExtension(const Extension* extension) {
  CheckOnValidThread();
  extension_info_[extension->id()] = extension;

  // Our map has already added a reference. Balance the reference given at the
  // call-site.
  extension->Release();
}

void ExtensionInfoMap::RemoveExtension(const std::string& id) {
  CheckOnValidThread();
  Map::iterator iter = extension_info_.find(id);
  if (iter != extension_info_.end()) {
    extension_info_.erase(iter);
  } else {
    // NOTE: This can currently happen if we receive multiple unload
    // notifications, e.g. setting incognito-enabled state for a
    // disabled extension (e.g., via sync).  See
    // http://code.google.com/p/chromium/issues/detail?id=50582 .
    NOTREACHED() << id;
  }
}


std::string ExtensionInfoMap::GetNameForExtension(const std::string& id) const {
  Map::const_iterator iter = extension_info_.find(id);
  if (iter != extension_info_.end())
    return iter->second->name();
  else
    return std::string();
}

FilePath ExtensionInfoMap::GetPathForExtension(const std::string& id) const {
  Map::const_iterator iter = extension_info_.find(id);
  if (iter != extension_info_.end())
    return iter->second->path();
  else
    return FilePath();
}

bool ExtensionInfoMap::ExtensionHasWebExtent(const std::string& id) const {
  Map::const_iterator iter = extension_info_.find(id);
  return iter != extension_info_.end() &&
      !iter->second->web_extent().is_empty();
}

bool ExtensionInfoMap::ExtensionCanLoadInIncognito(
    const std::string& id) const {
  Map::const_iterator iter = extension_info_.find(id);
  // Only split-mode extensions can load in incognito profiles.
  return iter != extension_info_.end() && iter->second->incognito_split_mode();
}

std::string ExtensionInfoMap::GetDefaultLocaleForExtension(
    const std::string& id) const {
  Map::const_iterator iter = extension_info_.find(id);
  std::string result;
  if (iter != extension_info_.end())
    result = iter->second->default_locale();

  return result;
}

ExtensionExtent ExtensionInfoMap::GetEffectiveHostPermissionsForExtension(
    const std::string& id) const {
  Map::const_iterator iter = extension_info_.find(id);
  ExtensionExtent result;
  if (iter != extension_info_.end())
    result = iter->second->GetEffectiveHostPermissions();

  return result;
}

bool ExtensionInfoMap::CheckURLAccessToExtensionPermission(
    const GURL& url,
    const char* permission_name) const {
  Map::const_iterator info;
  if (url.SchemeIs(chrome::kExtensionScheme)) {
    // If the url is an extension scheme, we just look it up by extension id.
    std::string id = url.host();
    info = extension_info_.find(id);
  } else {
    // Otherwise, we scan for a matching extent. Overlapping extents are
    // disallowed, so only one will match.
    info = extension_info_.begin();
    while (info != extension_info_.end() &&
           !info->second->web_extent().ContainsURL(url))
      ++info;
  }

  if (info == extension_info_.end())
    return false;

  return info->second->api_permissions().count(permission_name) != 0;
}

bool ExtensionInfoMap::URLIsForExtensionIcon(const GURL& url) const {
  DCHECK(url.SchemeIs(chrome::kExtensionScheme));

  Map::const_iterator iter = extension_info_.find(url.host());
  if (iter == extension_info_.end())
    return false;

  std::string path = url.path();
  DCHECK(path.length() > 0 && path[0] == '/');
  path = path.substr(1);
  return iter->second->icons().ContainsPath(path);
}
