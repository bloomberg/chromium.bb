// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_special_storage_policy.h"

#include "base/logging.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"

ExtensionSpecialStoragePolicy::ExtensionSpecialStoragePolicy() {}

ExtensionSpecialStoragePolicy::~ExtensionSpecialStoragePolicy() {}

bool ExtensionSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  if (origin.SchemeIs(chrome::kExtensionScheme))
    return true;
  base::AutoLock locker(lock_);
  return protected_apps_.Contains(origin);
}

bool ExtensionSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  base::AutoLock locker(lock_);
  return unlimited_extensions_.Contains(origin);
}

bool ExtensionSpecialStoragePolicy::IsFileHandler(
    const std::string& extension_id) {
  base::AutoLock locker(lock_);
  return file_handler_extensions_.ContainsExtension(extension_id);
}

void ExtensionSpecialStoragePolicy::GrantRightsForExtension(
    const Extension* extension) {
  DCHECK(extension);
  if (!extension->is_hosted_app() &&
      !extension->HasApiPermission(Extension::kUnlimitedStoragePermission) &&
      !extension->HasApiPermission(Extension::kFileBrowserHandlerPermission)) {
    return;
  }
  base::AutoLock locker(lock_);
  if (extension->is_hosted_app())
    protected_apps_.Add(extension);
  if (extension->HasApiPermission(Extension::kUnlimitedStoragePermission))
    unlimited_extensions_.Add(extension);
  if (extension->HasApiPermission(Extension::kFileBrowserHandlerPermission))
    file_handler_extensions_.Add(extension);
}

void ExtensionSpecialStoragePolicy::RevokeRightsForExtension(
    const Extension* extension) {
  DCHECK(extension);
  if (!extension->is_hosted_app() &&
      !extension->HasApiPermission(Extension::kUnlimitedStoragePermission) &&
      !extension->HasApiPermission(Extension::kFileBrowserHandlerPermission)) {
    return;
  }
  base::AutoLock locker(lock_);
  if (extension->is_hosted_app())
    protected_apps_.Remove(extension);
  if (extension->HasApiPermission(Extension::kUnlimitedStoragePermission))
    unlimited_extensions_.Remove(extension);
  if (extension->HasApiPermission(Extension::kFileBrowserHandlerPermission))
    file_handler_extensions_.Remove(extension);
}

void ExtensionSpecialStoragePolicy::RevokeRightsForAllExtensions() {
  base::AutoLock locker(lock_);
  protected_apps_.Clear();
  unlimited_extensions_.Clear();
  file_handler_extensions_.Clear();
}

//-----------------------------------------------------------------------------
// SpecialCollection helper class
//-----------------------------------------------------------------------------

ExtensionSpecialStoragePolicy::SpecialCollection::SpecialCollection() {}

ExtensionSpecialStoragePolicy::SpecialCollection::~SpecialCollection() {}

bool ExtensionSpecialStoragePolicy::SpecialCollection::Contains(
    const GURL& origin) {
  CachedResults::const_iterator found = cached_results_.find(origin);
  if (found != cached_results_.end())
    return found->second;

  for (Extensions::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    if (iter->second->OverlapsWithOrigin(origin)) {
      cached_results_[origin] = true;
      return true;
    }
  }
  cached_results_[origin] = false;
  return false;
}

bool ExtensionSpecialStoragePolicy::SpecialCollection::ContainsExtension(
    const std::string& extension_id) {
  return extensions_.find(extension_id) != extensions_.end();
}

void ExtensionSpecialStoragePolicy::SpecialCollection::Add(
    const Extension* extension) {
  cached_results_.clear();
  extensions_[extension->id()] = extension;
}

void ExtensionSpecialStoragePolicy::SpecialCollection::Remove(
    const Extension* extension) {
  cached_results_.clear();
  extensions_.erase(extension->id());
}

void ExtensionSpecialStoragePolicy::SpecialCollection::Clear() {
  cached_results_.clear();
  extensions_.clear();
}
