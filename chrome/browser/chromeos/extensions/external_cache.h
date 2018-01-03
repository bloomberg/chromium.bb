// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
class FilePath;
class Value;
}

namespace chromeos {

// The ExternalCache manages a cache for external extensions.
class ExternalCache {
 public:
  using PutExternalExtensionCallback =
      base::OnceCallback<void(const std::string& id, bool success)>;

  virtual ~ExternalCache() = default;

  // If an external extension should be downloaded, returns the extension's
  // update URL. Otherwise, returns an empty URL.
  static GURL GetExtensionUpdateUrl(const base::Value& extension_value,
                                    bool always_checking_for_updates);

  // Converts an external extension value to the external extension value
  // describing a cached extension - i.e. a value describing an extension
  // returned by GetCachedExtensions().
  static base::Value GetExtensionValueToCache(const base::Value& original_value,
                                              const std::string& path,
                                              const std::string& version);

  // If the external extension is not curently cached, whether the extension's
  // value should be added to the set of cached extensions (returned by
  // GetCachedExtensions()) regardles of the extension's download status.
  static bool ShouldCacheImmediately(const base::Value& extension_value,
                                     const std::string& installed_version);

  // Returns already cached extensions.
  virtual const base::DictionaryValue* GetCachedExtensions() = 0;

  // Shut down the cache. The |callback| will be invoked when the cache has shut
  // down completely and there are no more pending file I/O operations.
  virtual void Shutdown(base::OnceClosure callback) = 0;

  // Replace the list of extensions to cache with |prefs| and perform update
  // checks for these.
  virtual void UpdateExtensionsList(
      std::unique_ptr<base::DictionaryValue> prefs) = 0;

  // If a user of one of the ExternalCache's extensions detects that
  // the extension is damaged then this method can be used to remove it from
  // the cache and retry to download it after a restart.
  virtual void OnDamagedFileDetected(const base::FilePath& path) = 0;

  // Removes extensions listed in |ids| from external cache, corresponding crx
  // files will be removed from disk too.
  virtual void RemoveExtensions(const std::vector<std::string>& ids) = 0;

  // If extension with |id| exists in the cache, returns |true|, |file_path| and
  // |version| for the extension. Extension will be marked as used with current
  // timestamp.
  virtual bool GetExtension(const std::string& id,
                            base::FilePath* file_path,
                            std::string* version) = 0;

  // Whether the extension with the provided id is currently being cached -
  // i.e. whether the extension have been added to the external cache, but it's
  // data has not yet been fetched, and thus is not available using
  // GetExtension().
  virtual bool ExtensionFetchPending(const std::string& id) = 0;

  // Puts the external |crx_file_path| into |local_cache_| for extension with
  // |id|.
  virtual void PutExternalExtension(const std::string& id,
                                    const base::FilePath& crx_file_path,
                                    const std::string& version,
                                    PutExternalExtensionCallback callback) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_H_
