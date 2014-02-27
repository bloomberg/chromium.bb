// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_cache.h"

#include "base/memory/singleton.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/updater/extension_cache_impl.h"
#endif  // OS_CHROMEOS

namespace extensions {
namespace {

#if !defined(OS_CHROMEOS)

// Implementation of ExtensionCache that doesn't cache anything.
// Real cache is used only on Chrome OS other OSes use this null implementation.
class ExtensionCacheNullImpl : public ExtensionCache {
 public:
  static ExtensionCacheNullImpl* GetInstance() {
    return Singleton<ExtensionCacheNullImpl>::get();
  }

  // Implementation of ExtensionCache.
  virtual void Start(const base::Closure& callback) OVERRIDE {
    callback.Run();
  }

  virtual void Shutdown(const base::Closure& callback) OVERRIDE {
    callback.Run();
  }

  virtual void AllowCaching(const std::string& id) OVERRIDE {
  }

  virtual bool GetExtension(const std::string& id,
                            base::FilePath* file_path,
                            std::string* version) OVERRIDE {
    return false;
  }

  virtual void PutExtension(const std::string& id,
                            const base::FilePath& file_path,
                            const std::string& version,
                            const PutExtensionCallback& callback) OVERRIDE {
    callback.Run(file_path, true);
  }

 private:
  friend struct DefaultSingletonTraits<ExtensionCacheNullImpl>;

  ExtensionCacheNullImpl() {}
  virtual ~ExtensionCacheNullImpl() {}
};

#endif  // !OS_CHROMEOS

static ExtensionCache* g_extension_cache_override = NULL;

}  // namespace

// static
ExtensionCache* ExtensionCache::GetInstance() {
  if (g_extension_cache_override) {
    return g_extension_cache_override;
  } else {
#if defined(OS_CHROMEOS)
    return ExtensionCacheImpl::GetInstance();
#else
    return ExtensionCacheNullImpl::GetInstance();
#endif
  }
}

// static
ExtensionCache* ExtensionCache::SetForTesting(ExtensionCache* cache) {
  ExtensionCache* temp = g_extension_cache_override;
  g_extension_cache_override = cache;
  return temp;
}

}  // namespace extensions
