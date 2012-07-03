// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_namespace.h"
#include "chrome/browser/extensions/settings/settings_sync_processor.h"
#include "chrome/browser/extensions/settings/settings_sync_util.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/extension_setting_specifics.pb.h"

using content::BrowserThread;

namespace extensions {

SettingsSyncProcessor::SettingsSyncProcessor(
    const std::string& extension_id,
    syncer::ModelType type,
    syncer::SyncChangeProcessor* sync_processor)
    : extension_id_(extension_id),
      type_(type),
      sync_processor_(sync_processor),
      initialized_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK(type == syncer::EXTENSION_SETTINGS || type == syncer::APP_SETTINGS);
  CHECK(sync_processor);
}

SettingsSyncProcessor::~SettingsSyncProcessor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

void SettingsSyncProcessor::Init(const DictionaryValue& initial_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK(!initialized_) << "Init called multiple times";

  for (DictionaryValue::Iterator i(initial_state); i.HasNext(); i.Advance())
    synced_keys_.insert(i.key());

  initialized_ = true;
}

syncer::SyncError SettingsSyncProcessor::SendChanges(
    const ValueStoreChangeList& changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK(initialized_) << "Init not called";

  syncer::SyncChangeList sync_changes;
  std::set<std::string> added_keys;
  std::set<std::string> deleted_keys;

  for (ValueStoreChangeList::const_iterator i = changes.begin();
      i != changes.end(); ++i) {
    const std::string& key = i->key();
    const Value* value = i->new_value();
    if (value) {
      if (synced_keys_.count(key)) {
        // New value, key is synced; send ACTION_UPDATE.
        sync_changes.push_back(settings_sync_util::CreateUpdate(
            extension_id_, key, *value, type_));
      } else {
        // New value, key is not synced; send ACTION_ADD.
        sync_changes.push_back(settings_sync_util::CreateAdd(
            extension_id_, key, *value, type_));
        added_keys.insert(key);
      }
    } else {
      if (synced_keys_.count(key)) {
        // Clearing value, key is synced; send ACTION_DELETE.
        sync_changes.push_back(settings_sync_util::CreateDelete(
            extension_id_, key, type_));
        deleted_keys.insert(key);
      } else {
        LOG(WARNING) << "Deleted " << key << " but not in synced_keys_";
      }
    }
  }

  if (sync_changes.empty())
    return syncer::SyncError();

  syncer::SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, sync_changes);
  if (error.IsSet())
    return error;

  synced_keys_.insert(added_keys.begin(), added_keys.end());
  for (std::set<std::string>::iterator i = deleted_keys.begin();
      i != deleted_keys.end(); ++i) {
    synced_keys_.erase(*i);
  }

  return syncer::SyncError();
}

void SettingsSyncProcessor::NotifyChanges(const ValueStoreChangeList& changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK(initialized_) << "Init not called";

  for (ValueStoreChangeList::const_iterator i = changes.begin();
      i != changes.end(); ++i) {
    if (i->new_value())
      synced_keys_.insert(i->key());
    else
      synced_keys_.erase(i->key());
  }
}

}  // namespace extensions
