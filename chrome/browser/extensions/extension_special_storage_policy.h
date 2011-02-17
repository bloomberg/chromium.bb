// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SPECIAL_STORAGE_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SPECIAL_STORAGE_POLICY_H_
#pragma once

#include <map>
#include <set>

#include "base/synchronization/lock.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/special_storage_policy.h"

class Extension;

// Special rights are granted to 'extensions' and 'applications'. The
// storage subsystems and the browsing data remover query this interface
// to determine which origins have these rights.
class ExtensionSpecialStoragePolicy : public quota::SpecialStoragePolicy {
 public:
  ExtensionSpecialStoragePolicy();

  // SpecialStoragePolicy methods used by storage subsystems and the browsing
  // data remover. These methods are safe to call on any thread.
  virtual bool IsStorageProtected(const GURL& origin);
  virtual bool IsStorageUnlimited(const GURL& origin);

  // Methods used by the ExtensionService to populate this class.
  void GrantRightsForExtension(const Extension* extension);
  void RevokeRightsForExtension(const Extension* extension);
  void RevokeRightsForAllExtensions();

 private:
  friend class base::RefCountedThreadSafe<SpecialStoragePolicy>;
  virtual ~ExtensionSpecialStoragePolicy();

  class SpecialCollection {
   public:
    SpecialCollection();
    ~SpecialCollection();

    bool Contains(const GURL& origin);
    void Add(const Extension* extension);
    void Remove(const Extension* extension);
    void Clear();

   private:
    typedef std::map<GURL, bool> CachedResults;
    typedef std::set<const Extension*> Extensions;
    Extensions extensions_;
    CachedResults cached_resuts_;
  };

  base::Lock lock_;  // Synchronize all access to the collections.
  SpecialCollection protected_apps_;
  SpecialCollection unlimited_extensions_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SPECIAL_STORAGE_POLICY_H_
