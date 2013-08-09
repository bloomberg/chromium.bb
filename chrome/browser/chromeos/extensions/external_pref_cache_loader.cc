// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_pref_cache_loader.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

// Directory where the extensions are cached.
const char kPreinstalledAppsCacheDir[] = "/var/cache/external_cache";

}  // namespace

ExternalPrefCacheLoader::ExternalPrefCacheLoader(int base_path_id,
                                                 Options options)
  : ExternalPrefLoader(base_path_id, options) {
}

ExternalPrefCacheLoader::~ExternalPrefCacheLoader() {
}

void ExternalPrefCacheLoader::OnExtensionListsUpdated(
    const base::DictionaryValue* prefs) {
  prefs_.reset(prefs->DeepCopy());
  ExternalPrefLoader::LoadFinished();
}

void ExternalPrefCacheLoader::LoadFinished() {
  if (!external_cache_.get()) {
    external_cache_.reset(new ExternalCache(kPreinstalledAppsCacheDir,
        g_browser_process->system_request_context(),
        this, true));
  }

  external_cache_->UpdateExtensionsList(prefs_.Pass());
}

}  // namespace chromeos
