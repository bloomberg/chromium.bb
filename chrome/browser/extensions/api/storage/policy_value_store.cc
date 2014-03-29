// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/policy_value_store.h"

#include "base/logging.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/value_store/value_store_change.h"
#include "extensions/browser/value_store/value_store_util.h"

using content::BrowserThread;

namespace util = value_store_util;

namespace extensions {

namespace {

scoped_ptr<ValueStore::Error> ReadOnlyError(scoped_ptr<std::string> key) {
  return make_scoped_ptr(new ValueStore::Error(
      ValueStore::READ_ONLY, "This is a read-only store.", key.Pass()));
}

}  // namespace

PolicyValueStore::PolicyValueStore(
    const std::string& extension_id,
    const scoped_refptr<SettingsObserverList>& observers,
    scoped_ptr<ValueStore> delegate)
    : extension_id_(extension_id),
      observers_(observers),
      delegate_(delegate.Pass()) {}

PolicyValueStore::~PolicyValueStore() {}

void PolicyValueStore::SetCurrentPolicy(const policy::PolicyMap& policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  // Convert |policy| to a dictionary value. Only include mandatory policies
  // for now.
  base::DictionaryValue current_policy;
  for (policy::PolicyMap::const_iterator it = policy.begin();
       it != policy.end(); ++it) {
    if (it->second.level == policy::POLICY_LEVEL_MANDATORY) {
      current_policy.SetWithoutPathExpansion(
          it->first, it->second.value->DeepCopy());
    }
  }

  // Get the previous policies stored in the database.
  // TODO(joaodasilva): it'd be better to have a less expensive way of
  // determining which keys are currently stored, or of determining which keys
  // must be removed.
  base::DictionaryValue previous_policy;
  ValueStore::ReadResult read_result = delegate_->Get();

  // If the database is corrupted, try to restore it.
  // This may have the unfortunate side-effect of incorrectly informing the
  // extension of a "new" key, which isn't new and was corrupted. Unfortunately,
  // there's not always a way around this - if the database is corrupted, there
  // may be no way of telling which keys were previously present.
  if (read_result->IsCorrupted()) {
    if (delegate_->Restore())
      read_result = delegate_->Get();
  }

  if (read_result->HasError()) {
    LOG(WARNING) << "Failed to read managed settings for extension "
        << extension_id_ << ": " << read_result->error().message;
    // Leave |previous_policy| empty, so that events are generated for every
    // policy in |current_policy|.
  } else {
    read_result->settings().Swap(&previous_policy);
  }

  // Now get two lists of changes: changes after setting the current policies,
  // and changes after removing old policies that aren't in |current_policy|
  // anymore.
  std::vector<std::string> removed_keys;
  for (base::DictionaryValue::Iterator it(previous_policy);
       !it.IsAtEnd(); it.Advance()) {
    if (!current_policy.HasKey(it.key()))
      removed_keys.push_back(it.key());
  }

  ValueStoreChangeList changes;

  WriteResult result = delegate_->Remove(removed_keys);
  if (!result->HasError()) {
    changes.insert(
        changes.end(), result->changes().begin(), result->changes().end());
  }

  // IGNORE_QUOTA because these settings aren't writable by the extension, and
  // are configured by the domain administrator.
  ValueStore::WriteOptions options = ValueStore::IGNORE_QUOTA;
  result = delegate_->Set(options, current_policy);
  if (!result->HasError()) {
    changes.insert(
        changes.end(), result->changes().begin(), result->changes().end());
  }

  if (!changes.empty()) {
    observers_->Notify(
        &SettingsObserver::OnSettingsChanged,
        extension_id_,
        settings_namespace::MANAGED,
        ValueStoreChange::ToJson(changes));
  }
}

void PolicyValueStore::DeleteStorage() {
  // This is called from our owner, indicating that storage for this extension
  // should be removed.
  delegate_->Clear();
}

size_t PolicyValueStore::GetBytesInUse(const std::string& key) {
  // LeveldbValueStore doesn't implement this; and the underlying database
  // isn't acccessible to the extension in any case; from the extension's
  // perspective this is a read-only store.
  return 0;
}

size_t PolicyValueStore::GetBytesInUse(const std::vector<std::string>& keys) {
  // See note above.
  return 0;
}

size_t PolicyValueStore::GetBytesInUse() {
  // See note above.
  return 0;
}

ValueStore::ReadResult PolicyValueStore::Get(const std::string& key) {
  return delegate_->Get(key);
}

ValueStore::ReadResult PolicyValueStore::Get(
    const std::vector<std::string>& keys) {
  return delegate_->Get(keys);
}

ValueStore::ReadResult PolicyValueStore::Get() {
  return delegate_->Get();
}

ValueStore::WriteResult PolicyValueStore::Set(
    WriteOptions options, const std::string& key, const base::Value& value) {
  return MakeWriteResult(ReadOnlyError(util::NewKey(key)));
}

ValueStore::WriteResult PolicyValueStore::Set(
    WriteOptions options, const base::DictionaryValue& settings) {
  return MakeWriteResult(ReadOnlyError(util::NoKey()));
}

ValueStore::WriteResult PolicyValueStore::Remove(const std::string& key) {
  return MakeWriteResult(ReadOnlyError(util::NewKey(key)));
}

ValueStore::WriteResult PolicyValueStore::Remove(
    const std::vector<std::string>& keys) {
  return MakeWriteResult(ReadOnlyError(util::NoKey()));
}

ValueStore::WriteResult PolicyValueStore::Clear() {
  return MakeWriteResult(ReadOnlyError(util::NoKey()));
}

bool PolicyValueStore::Restore() { return delegate_->Restore(); }

bool PolicyValueStore::RestoreKey(const std::string& key) {
  return delegate_->RestoreKey(key);
}

}  // namespace extensions
