// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_special_storage_policy.h"

#include "base/logging.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"

ExtensionSpecialStoragePolicy::ExtensionSpecialStoragePolicy() {}

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

void ExtensionSpecialStoragePolicy::GrantRightsForExtension(
    const Extension* extension) {
  DCHECK(extension);
  if (!extension->is_hosted_app() &&
      !extension->HasApiPermission(Extension::kUnlimitedStoragePermission)) {
    return;
  }
  base::AutoLock locker(lock_);
  if (extension->is_hosted_app())
    protected_apps_.Add(extension);
  if (extension->HasApiPermission(Extension::kUnlimitedStoragePermission))
    unlimited_extensions_.Add(extension);
}

void ExtensionSpecialStoragePolicy::RevokeRightsForExtension(
    const Extension* extension) {
  DCHECK(extension);
  if (!extension->is_hosted_app() &&
      !extension->HasApiPermission(Extension::kUnlimitedStoragePermission)) {
    return;
  }
  base::AutoLock locker(lock_);
  if (extension->is_hosted_app())
    protected_apps_.Remove(extension);
  if (extension->HasApiPermission(Extension::kUnlimitedStoragePermission))
    unlimited_extensions_.Remove(extension);
}

void ExtensionSpecialStoragePolicy::RevokeRightsForAllExtensions() {
  base::AutoLock locker(lock_);
  protected_apps_.Clear();
  unlimited_extensions_.Clear();
}

ExtensionSpecialStoragePolicy::~ExtensionSpecialStoragePolicy() {}

//-----------------------------------------------------------------------------
// SpecialCollection helper class
//-----------------------------------------------------------------------------

ExtensionSpecialStoragePolicy::SpecialCollection::SpecialCollection() {}

ExtensionSpecialStoragePolicy::SpecialCollection::~SpecialCollection() {}

bool ExtensionSpecialStoragePolicy::SpecialCollection::Contains(
    const GURL& origin) {
  CachedResults::const_iterator found = cached_resuts_.find(origin);
  if (found != cached_resuts_.end())
    return found->second;

  for (Extensions::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    if ((*iter)->OverlapsWithOrigin(origin)) {
      cached_resuts_[origin] = true;
      return true;
    }
  }
  cached_resuts_[origin] = false;
  return false;
}

void ExtensionSpecialStoragePolicy::SpecialCollection::Add(
    const Extension* extension) {
  cached_resuts_.clear();
  extensions_.insert(extension);
}

void ExtensionSpecialStoragePolicy::SpecialCollection::Remove(
    const Extension* extension) {
  cached_resuts_.clear();
  extensions_.erase(extension);
}

void ExtensionSpecialStoragePolicy::SpecialCollection::Clear() {
  cached_resuts_.clear();
  extensions_.clear();
}
