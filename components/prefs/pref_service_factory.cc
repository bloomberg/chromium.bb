// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_service_factory.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_store.h"

namespace {

// Do-nothing default implementation.
void DoNothingHandleReadError(PersistentPrefStore::PrefReadError error) {
}

}  // namespace

PrefServiceFactory::PrefServiceFactory()
    : read_error_callback_(base::Bind(&DoNothingHandleReadError)),
      async_(false) {}

PrefServiceFactory::~PrefServiceFactory() {}

void PrefServiceFactory::SetUserPrefsFile(
    const base::FilePath& prefs_file,
    base::SequencedTaskRunner* task_runner) {
  user_prefs_ =
      base::MakeRefCounted<JsonPrefStore>(prefs_file, task_runner, nullptr);
}

std::unique_ptr<PrefService> PrefServiceFactory::Create(
    PrefRegistry* pref_registry,
    std::unique_ptr<PrefValueStore::Delegate> delegate) {
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();
  std::unique_ptr<PrefService> pref_service(new PrefService(
      pref_notifier,
      new PrefValueStore(managed_prefs_.get(), supervised_user_prefs_.get(),
                         extension_prefs_.get(), command_line_prefs_.get(),
                         user_prefs_.get(), recommended_prefs_.get(),
                         pref_registry->defaults().get(), pref_notifier,
                         std::move(delegate)),
      user_prefs_.get(), pref_registry, read_error_callback_, async_));
  return pref_service;
}
