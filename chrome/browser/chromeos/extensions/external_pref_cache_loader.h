// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_PREF_CACHE_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_PREF_CACHE_LOADER_H_

#include "chrome/browser/chromeos/extensions/external_cache.h"
#include "chrome/browser/extensions/external_pref_loader.h"

namespace chromeos {

// A specialization of the ExternalPrefCacheLoader that caches crx files for
// external extensions with update URL in common place for all users on the
// machine.
class ExternalPrefCacheLoader : public extensions::ExternalPrefLoader,
                                public ExternalCache::Delegate {
 public:
  ExternalPrefCacheLoader(int base_path_id, Options options);

  // Implementation of ExternalCache::Delegate:
  virtual void OnExtensionListsUpdated(
      const base::DictionaryValue* prefs) OVERRIDE;

 protected:
  virtual ~ExternalPrefCacheLoader();

  virtual void LoadFinished() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  scoped_ptr<ExternalCache> external_cache_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPrefCacheLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_PREF_CACHE_LOADER_H_
