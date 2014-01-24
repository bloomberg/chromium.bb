// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_CACHE_FAKE_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_CACHE_FAKE_H_

#include <map>
#include <set>
#include <string>

#include "chrome/browser/extensions/updater/extension_cache.h"

namespace extensions {

// Fake implementation of extensions ExtensionCache that can be used in tests.
class ExtensionCacheFake : public ExtensionCache {
 public:
  ExtensionCacheFake();
  virtual ~ExtensionCacheFake();

  // Implementation of ExtensionCache.
  virtual void Start(const base::Closure& callback) OVERRIDE;
  virtual void Shutdown(const base::Closure& callback) OVERRIDE;
  virtual void AllowCaching(const std::string& id) OVERRIDE;
  virtual bool GetExtension(const std::string& id,
                            base::FilePath* file_path,
                            std::string* version) OVERRIDE;
  virtual void PutExtension(const std::string& id,
                            const base::FilePath& file_path,
                            const std::string& version,
                            const PutExtensionCallback& callback) OVERRIDE;

 private:
  typedef std::map<std::string, std::pair<std::string, base::FilePath> > Map;
  // Set of extensions that can be cached.
  std::set<std::string> allowed_extensions_;

  // Map of know extensions.
  Map cache_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCacheFake);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_CACHE_FAKE_H_
