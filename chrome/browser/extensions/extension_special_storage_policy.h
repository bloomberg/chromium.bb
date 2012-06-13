// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SPECIAL_STORAGE_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SPECIAL_STORAGE_POLICY_H_
#pragma once

#include <map>
#include <string>

#include "base/synchronization/lock.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/special_storage_policy.h"

class CookieSettings;

namespace extensions {
class Extension;
}

// Special rights are granted to 'extensions' and 'applications'. The
// storage subsystems and the browsing data remover query this interface
// to determine which origins have these rights.
class ExtensionSpecialStoragePolicy : public quota::SpecialStoragePolicy {
 public:
  explicit ExtensionSpecialStoragePolicy(CookieSettings* cookie_settings);

  // quota::SpecialStoragePolicy methods used by storage subsystems and the
  // browsing data remover. These methods are safe to call on any thread.
  virtual bool IsStorageProtected(const GURL& origin) OVERRIDE;
  virtual bool IsStorageUnlimited(const GURL& origin) OVERRIDE;
  virtual bool IsStorageSessionOnly(const GURL& origin) OVERRIDE;
  virtual bool IsFileHandler(const std::string& extension_id) OVERRIDE;
  virtual bool HasSessionOnlyOrigins() OVERRIDE;

  // Methods used by the ExtensionService to populate this class.
  void GrantRightsForExtension(const extensions::Extension* extension);
  void RevokeRightsForExtension(const extensions::Extension* extension);
  void RevokeRightsForAllExtensions();

  // Decides whether the storage for |extension|'s web extent needs protection.
  bool NeedsProtection(const extensions::Extension* extension);

 protected:
  virtual ~ExtensionSpecialStoragePolicy();

 private:
  class SpecialCollection {
   public:
    SpecialCollection();
    ~SpecialCollection();

    bool Contains(const GURL& origin);
    bool ContainsExtension(const std::string& extension_id);
    void Add(const extensions::Extension* extension);
    void Remove(const extensions::Extension* extension);
    void Clear();

   private:
    typedef std::map<GURL, bool> CachedResults;
    typedef std::map<std::string, scoped_refptr<const extensions::Extension> >
        Extensions;
    Extensions extensions_;
    CachedResults cached_results_;
  };

  void NotifyChanged();

  base::Lock lock_;  // Synchronize all access to the collections.
  SpecialCollection protected_apps_;
  SpecialCollection unlimited_extensions_;
  SpecialCollection file_handler_extensions_;
  scoped_refptr<CookieSettings> cookie_settings_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SPECIAL_STORAGE_POLICY_H_
