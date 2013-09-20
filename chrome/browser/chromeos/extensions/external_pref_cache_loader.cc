// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_pref_cache_loader.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/external_cache.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

// Directory where the extensions are cached.
const char kPreinstalledAppsCacheDir[] = "/var/cache/external_cache";

// Singleton class that holds ExternalCache and dispatches cache update events
// to per-profile instances of ExternalPrefCacheLoader. This multiplexing
// is required for multi-profile case.
class ExternalCacheDispatcher : public ExternalCache::Delegate {
 public:
  static ExternalCacheDispatcher* GetInstance() {
    return Singleton<ExternalCacheDispatcher>::get();
  }

  // Implementation of ExternalCache::Delegate:
  virtual void OnExtensionListsUpdated(
      const base::DictionaryValue* prefs) OVERRIDE {
    is_extensions_list_ready_ = true;
    FOR_EACH_OBSERVER(ExternalPrefCacheLoader, pref_loaders_,
                      OnExtensionListsUpdated(prefs));
  }

  void UpdateExtensionsList(scoped_ptr<base::DictionaryValue> prefs) {
    DCHECK(!is_extensions_list_ready_);
    external_cache_.UpdateExtensionsList(prefs.Pass());
  }

  // Return false if cache doesn't have list of extensions and it needs to
  // be provided via UpdateExtensionsList.
  bool RegisterExternalPrefCacheLoader(ExternalPrefCacheLoader* observer,
                                       int base_path_id) {
    pref_loaders_.AddObserver(observer);

    if (base_path_id_ == 0) {
      // First ExternalPrefCacheLoader is registered.
      base_path_id_ = base_path_id;
      return false;
    } else {
      CHECK_EQ(base_path_id_, base_path_id);
      if (is_extensions_list_ready_) {
        // If list of extensions is not ready, |observer| will be notified later
        // in OnExtensionListsUpdated.
        observer->OnExtensionListsUpdated(external_cache_.cached_extensions());
      }
      return true;
    }
  }

  void UnregisterExternalPrefCacheLoader(ExternalPrefCacheLoader* observer) {
    pref_loaders_.RemoveObserver(observer);
  }

 private:
  friend struct DefaultSingletonTraits<ExternalCacheDispatcher>;

  ExternalCacheDispatcher()
    : external_cache_(kPreinstalledAppsCacheDir,
                      g_browser_process->system_request_context(),
                      this, true, true),
      base_path_id_(0),
      is_extensions_list_ready_(false) {
  }

  ExternalCache external_cache_;
  ObserverList<ExternalPrefCacheLoader> pref_loaders_;
  int base_path_id_;
  bool is_extensions_list_ready_;

  DISALLOW_COPY_AND_ASSIGN(ExternalCacheDispatcher);
};

}  // namespace

ExternalPrefCacheLoader::ExternalPrefCacheLoader(int base_path_id)
  : ExternalPrefLoader(base_path_id, ExternalPrefLoader::NONE) {
}

ExternalPrefCacheLoader::~ExternalPrefCacheLoader() {
  ExternalCacheDispatcher::GetInstance()->UnregisterExternalPrefCacheLoader(
      this);
}

void ExternalPrefCacheLoader::OnExtensionListsUpdated(
    const base::DictionaryValue* prefs) {
  prefs_.reset(prefs->DeepCopy());
  ExternalPrefLoader::LoadFinished();
}

void ExternalPrefCacheLoader::StartLoading() {
  if (!ExternalCacheDispatcher::GetInstance()->RegisterExternalPrefCacheLoader(
          this, base_path_id_)) {
    // ExternalCacheDispatcher doesn't know list of extensions load it.
    ExternalPrefLoader::StartLoading();
  }
}

void ExternalPrefCacheLoader::LoadFinished() {
  ExternalCacheDispatcher::GetInstance()->UpdateExtensionsList(prefs_.Pass());
}

}  // namespace chromeos
