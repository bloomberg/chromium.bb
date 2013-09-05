// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_settings_service.h"

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/prefs/json_pref_store.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"

using base::DictionaryValue;
using base::Value;
using content::BrowserThread;
using content::UserMetricsAction;

const char kAtomicSettings[] = "atomic_settings";
const char kManagedUserInternalItemPrefix[] = "X-";
const char kQueuedItems[] = "queued_items";
const char kSplitSettingKeySeparator = ':';
const char kSplitSettings[] = "split_settings";

namespace {

bool SettingShouldApplyToPrefs(const std::string& name) {
  return !StartsWithASCII(name, kManagedUserInternalItemPrefix, false);
}

}  // namespace

ManagedUserSettingsService::ManagedUserSettingsService()
    : active_(false), local_settings_(new DictionaryValue) {}

ManagedUserSettingsService::~ManagedUserSettingsService() {}

void ManagedUserSettingsService::Init(
    scoped_refptr<PersistentPrefStore> store) {
  DCHECK(!store_);
  store_ = store;
  store_->AddObserver(this);
}

void ManagedUserSettingsService::Subscribe(const SettingsCallback& callback) {
  if (IsReady()) {
    scoped_ptr<base::DictionaryValue> settings = GetSettings();
    callback.Run(settings.get());
  }

  subscribers_.push_back(callback);
}

void ManagedUserSettingsService::Activate() {
  active_ = true;
  InformSubscribers();
}

bool ManagedUserSettingsService::IsReady() {
  return store_->IsInitializationComplete();
}

void ManagedUserSettingsService::Clear() {
  store_->RemoveValue(kAtomicSettings);
  store_->RemoveValue(kSplitSettings);
}
void ManagedUserSettingsService::SetLocalSettingForTesting(
    const std::string& key,
    scoped_ptr<Value> value) {
  if (value)
    local_settings_->SetWithoutPathExpansion(key, value.release());
  else
    local_settings_->RemoveWithoutPathExpansion(key, NULL);

  InformSubscribers();
}

void ManagedUserSettingsService::Shutdown() {
  store_->RemoveObserver(this);
}
void ManagedUserSettingsService::OnPrefValueChanged(const std::string& key) {}

void ManagedUserSettingsService::OnInitializationCompleted(bool success) {
  DCHECK(success);
  DCHECK(IsReady());
  InformSubscribers();
}

DictionaryValue* ManagedUserSettingsService::GetOrCreateDictionary(
    const std::string& key) const {
  Value* value = NULL;
  DictionaryValue* dict = NULL;
  if (store_->GetMutableValue(key, &value)) {
    bool success = value->GetAsDictionary(&dict);
    DCHECK(success);
  } else {
    dict = new base::DictionaryValue;
    store_->SetValue(key, dict);
  }

  return dict;
}

DictionaryValue* ManagedUserSettingsService::GetAtomicSettings() const {
  return GetOrCreateDictionary(kAtomicSettings);
}

DictionaryValue* ManagedUserSettingsService::GetSplitSettings() const {
  return GetOrCreateDictionary(kSplitSettings);
}

DictionaryValue* ManagedUserSettingsService::GetQueuedItems() const {
  return GetOrCreateDictionary(kQueuedItems);
}

scoped_ptr<DictionaryValue> ManagedUserSettingsService::GetSettings() {
  DCHECK(IsReady());
  if (!active_)
    return scoped_ptr<base::DictionaryValue>();

  scoped_ptr<DictionaryValue> settings(local_settings_->DeepCopy());

  DictionaryValue* atomic_settings = GetAtomicSettings();
  for (DictionaryValue::Iterator it(*atomic_settings); !it.IsAtEnd();
       it.Advance()) {
    if (!SettingShouldApplyToPrefs(it.key()))
      continue;

    settings->Set(it.key(), it.value().DeepCopy());
  }

  DictionaryValue* split_settings = GetSplitSettings();
  for (DictionaryValue::Iterator it(*split_settings); !it.IsAtEnd();
       it.Advance()) {
    if (!SettingShouldApplyToPrefs(it.key()))
      continue;

    settings->Set(it.key(), it.value().DeepCopy());
  }

  return settings.Pass();
}

void ManagedUserSettingsService::InformSubscribers() {
  if (!IsReady())
    return;

  scoped_ptr<base::DictionaryValue> settings = GetSettings();
  for (std::vector<SettingsCallback>::iterator it = subscribers_.begin();
       it != subscribers_.end(); ++it) {
    it->Run(settings.get());
  }
}
