// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/public/cpp/prefs/pref_service_factory.h"

#include "base/bind.h"
#include "components/filesystem/public/cpp/prefs/filesystem_json_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/value_map_pref_store.h"
#include "components/prefs/writeable_pref_store.h"
#include "mojo/shell/public/cpp/application_impl.h"

namespace filesystem {

namespace {

// Do-nothing default implementation.
void DoNothingHandleReadError(PersistentPrefStore::PrefReadError error) {}

}  // namespace

scoped_ptr<PrefService> CreatePrefService(mojo::ApplicationImpl* application,
                                          PrefRegistry* pref_registry) {
  filesystem::FileSystemPtr filesystem;
  application->ConnectToService("mojo:filesystem", &filesystem);

  scoped_refptr<FilesystemJsonPrefStore> user_prefs =
      new FilesystemJsonPrefStore("preferences.json", std::move(filesystem),
                                  nullptr /* TODO(erg): pref filter */);

  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();
  scoped_ptr<PrefService> pref_service(new PrefService(
      pref_notifier,
      new PrefValueStore(nullptr, nullptr, nullptr, nullptr, user_prefs.get(),
                         nullptr, pref_registry->defaults().get(),
                         pref_notifier),
      user_prefs.get(), pref_registry, base::Bind(&DoNothingHandleReadError),
      true /* async */));
  return pref_service;
}

}  // namespace filesystem
