// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_builder.h"

#include "base/bind.h"
#include "base/prefs/default_pref_store.h"
#include "base/prefs/json_pref_store.h"
#include "chrome/browser/prefs/pref_notifier_impl.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_store.h"

namespace {

// Do-nothing default implementation.
void DoNothingHandleReadError(PersistentPrefStore::PrefReadError error) {
}

}  // namespace

PrefServiceBuilder::PrefServiceBuilder()
    : sync_associator_(NULL) {
  ResetDefaultState();
}

PrefServiceBuilder::~PrefServiceBuilder() {
}

PrefServiceBuilder& PrefServiceBuilder::WithManagedPrefs(PrefStore* store) {
  managed_prefs_ = store;
  return *this;
}

PrefServiceBuilder& PrefServiceBuilder::WithExtensionPrefs(PrefStore* store) {
  extension_prefs_ = store;
  return *this;
}

PrefServiceBuilder& PrefServiceBuilder::WithCommandLinePrefs(PrefStore* store) {
  command_line_prefs_ = store;
  return *this;
}

PrefServiceBuilder& PrefServiceBuilder::WithUserPrefs(
    PersistentPrefStore* store) {
  user_prefs_ = store;
  return *this;
}

PrefServiceBuilder& PrefServiceBuilder::WithRecommendedPrefs(PrefStore* store) {
  recommended_prefs_ = store;
  return *this;
}

PrefServiceBuilder& PrefServiceBuilder::WithSyncAssociator(
    PrefModelAssociator* associator) {
  sync_associator_ = associator;
  return *this;
}

PrefServiceBuilder& PrefServiceBuilder::WithReadErrorCallback(
    const base::Callback<void(PersistentPrefStore::PrefReadError)>&
    read_error_callback) {
  read_error_callback_ = read_error_callback;
  return *this;
}

PrefServiceBuilder& PrefServiceBuilder::WithUserFilePrefs(
    const FilePath& prefs_file,
    base::SequencedTaskRunner* task_runner) {
  user_prefs_ = new JsonPrefStore(prefs_file, task_runner);
  return *this;
}

PrefServiceBuilder& PrefServiceBuilder::WithAsync(bool async) {
  async_ = async;
  return *this;
}

PrefService* PrefServiceBuilder::Create() {
  DefaultPrefStore* default_pref_store = new DefaultPrefStore();
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();
  PrefService* pref_service =
      new PrefService(
          pref_notifier,
          new PrefValueStore(
              managed_prefs_.get(),
              extension_prefs_.get(),
              command_line_prefs_.get(),
              user_prefs_.get(),
              recommended_prefs_.get(),
              default_pref_store,
              sync_associator_,
              pref_notifier),
          user_prefs_.get(),
          default_pref_store,
          sync_associator_,
          read_error_callback_,
          async_);
  managed_prefs_ = NULL;
  extension_prefs_ = NULL;
  command_line_prefs_ = NULL;
  user_prefs_ = NULL;
  recommended_prefs_ = NULL;
  sync_associator_ = NULL;
  ResetDefaultState();

  return pref_service;
}

void PrefServiceBuilder::ResetDefaultState() {
  read_error_callback_ = base::Bind(&DoNothingHandleReadError);
  async_ = false;
}
