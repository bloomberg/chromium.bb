// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_storage_cache.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/json/json_writer.h"

namespace {

void RemoveAllWithoutPathExpansion(
    DictionaryValue* from, const ListValue* keys_to_remove) {
  std::string key;
  for (ListValue::const_iterator it = keys_to_remove->begin();
      it != keys_to_remove->end(); ++it) {
    if ((*it)->GetAsString(&key)) {
      from->RemoveWithoutPathExpansion(key, NULL);
    }
  }
}

// Callback which delegates to a given callback but caches any result if
// successful.  Takes an optional parameter of existing settings to merge with
// before returning to the callback (may be NULL) and an optional parameter of
// keys to remove from the cache post-success (may be NULL).
class CacheModifyingCallback : public ExtensionSettingsStorage::Callback {
 public:
  // Takes ownership of callback, existing, and keys_to_remove.
  CacheModifyingCallback(
      ExtensionSettingsStorage::Callback* delegate,
      base::WeakPtr<DictionaryValue> cache,
      DictionaryValue* existing,
      ListValue* keys_to_remove)
      : delegate_(delegate),
        cache_(cache),
        existing_(existing),
        keys_to_remove_(keys_to_remove) {
  }

  static CacheModifyingCallback* Create(
      ExtensionSettingsStorage::Callback* delegate,
      base::WeakPtr<DictionaryValue> cache_ptr_) {
    return new CacheModifyingCallback(
        delegate,
        cache_ptr_,
        NULL,
        NULL);
  }

  static CacheModifyingCallback* Create(
      ExtensionSettingsStorage::Callback* delegate,
      base::WeakPtr<DictionaryValue> cache_ptr_,
      DictionaryValue* existing) {
    return new CacheModifyingCallback(
        delegate,
        cache_ptr_,
        existing,
        NULL);
  }

  static CacheModifyingCallback* Create(
      ExtensionSettingsStorage::Callback* delegate,
      base::WeakPtr<DictionaryValue> cache_ptr_,
      ListValue* remove) {
    return new CacheModifyingCallback(
        delegate,
        cache_ptr_,
        NULL,
        remove);
  }

  virtual void OnSuccess(DictionaryValue* settings) OVERRIDE {
    // Note checking that the weak reference to the cache is still valid.
    // It might be NULL if the owning ExtensionSettingsStorageCache has been
    // deleted.
    // Also, settings may be NULL for Remove/Clear.
    if (settings != NULL && cache_.get() != NULL) {
      cache_->MergeDictionary(settings);
    }
    if (settings != NULL && existing_ != NULL) {
      settings->MergeDictionary(existing_.get());
    }
    if (cache_.get() != NULL && keys_to_remove_ != NULL) {
      RemoveAllWithoutPathExpansion(cache_, keys_to_remove_.get());
    }
    delegate_->OnSuccess(settings);
  }

  virtual void OnFailure(const std::string& message) OVERRIDE {
    delegate_->OnFailure(message);
  }

 private:
  scoped_ptr<ExtensionSettingsStorage::Callback> delegate_;
  base::WeakPtr<DictionaryValue> cache_;
  scoped_ptr<DictionaryValue> existing_;
  scoped_ptr<ListValue> keys_to_remove_;
};

}  // namespace

ExtensionSettingsStorageCache::ExtensionSettingsStorageCache(
    ExtensionSettingsStorage* delegate)
    : delegate_(delegate), cache_ptr_factory_(&cache_) {
}

ExtensionSettingsStorageCache::~ExtensionSettingsStorageCache() {
}

void ExtensionSettingsStorageCache::DeleteSoon() {
  delegate_->DeleteSoon();
  delete this;
}

void ExtensionSettingsStorageCache::Get(const std::string& key,
    ExtensionSettingsStorageCache::Callback* callback) {
  Value *value;
  if (GetFromCache(key, &value)) {
    DictionaryValue* settings = new DictionaryValue();
    settings->SetWithoutPathExpansion(key, value);
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &ExtensionSettingsStorageCache::Callback::OnSuccess,
            base::Unretained(callback),
            settings));
  } else {
    delegate_->Get(
        key,
        CacheModifyingCallback::Create(
            callback,
            cache_ptr_factory_.GetWeakPtr()));
  }
}

void ExtensionSettingsStorageCache::Get(const ListValue& keys,
    ExtensionSettingsStorageCache::Callback* callback) {
  std::string key;
  DictionaryValue* settings = new DictionaryValue();
  ListValue missing_keys;

  for (ListValue::const_iterator it = keys.begin(); it != keys.end(); ++it) {
    if ((*it)->GetAsString(&key)) {
      Value *value;
      if (GetFromCache(key, &value)) {
        settings->SetWithoutPathExpansion(key, value);
      } else {
        missing_keys.Append(Value::CreateStringValue(key));
      }
    }
  }

  if (missing_keys.empty()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &ExtensionSettingsStorageCache::Callback::OnSuccess,
            base::Unretained(callback),
            settings));
  } else {
    delegate_->Get(
        missing_keys,
        CacheModifyingCallback::Create(
            callback,
            cache_ptr_factory_.GetWeakPtr(),
            settings));
  }
}

void ExtensionSettingsStorageCache::Get(
    ExtensionSettingsStorageCache::Callback* callback) {
  // Copy the cache when passing in, as a semi-hack so that caching a no-op
  // storage object works (which always returns empty from Get()).
  delegate_->Get(
      CacheModifyingCallback::Create(
          callback,
          cache_ptr_factory_.GetWeakPtr(),
          cache_.DeepCopy()));
}

void ExtensionSettingsStorageCache::Set(
    const std::string& key,
    const Value& value,
    ExtensionSettingsStorageCache::Callback* callback) {
  // Invalidate the cached entry first, in case the set fails.
  cache_.RemoveWithoutPathExpansion(key, NULL);
  delegate_->Set(
      key,
      value,
      CacheModifyingCallback::Create(
          callback,
          cache_ptr_factory_.GetWeakPtr()));
}

void ExtensionSettingsStorageCache::Set(
    const DictionaryValue& values,
    ExtensionSettingsStorageCache::Callback* callback) {
  // Invalidate the cached entries first, in case the set fails.
  for (DictionaryValue::key_iterator it = values.begin_keys();
      it != values.end_keys(); ++it) {
    cache_.RemoveWithoutPathExpansion(*it, NULL);
  }
  delegate_->Set(
      values,
      CacheModifyingCallback::Create(
        callback,
        cache_ptr_factory_.GetWeakPtr()));
}

void ExtensionSettingsStorageCache::Remove(
    const std::string& key,
    ExtensionSettingsStorageCache::Callback *callback) {
  // Invalidate the cached entry first, in case the remove fails.
  // We will also need to do if after the callback, to avoid race conditions
  // whether other API calls fill the cache on the UI thread.
  // This would be a good time to use structured cloning...
  cache_.RemoveWithoutPathExpansion(key, NULL);
  ListValue* key_list = new ListValue();
  key_list->Append(Value::CreateStringValue(key));
  delegate_->Remove(
      key,
      CacheModifyingCallback::Create(
        callback,
        cache_ptr_factory_.GetWeakPtr(),
        key_list));
}

void ExtensionSettingsStorageCache::Remove(
    const ListValue& keys,
    ExtensionSettingsStorageCache::Callback *callback) {
  std::string key;
  // Invalidate each cached entry first, in case the remove fails.
  // We will also need to do if after the callback, to avoid race conditions
  // whether other API calls fill the cache on the UI thread.
  RemoveAllWithoutPathExpansion(&cache_, &keys);
  delegate_->Remove(
      keys,
      CacheModifyingCallback::Create(
          callback,
          cache_ptr_factory_.GetWeakPtr(),
          keys.DeepCopy()));
}

void ExtensionSettingsStorageCache::Clear(
    ExtensionSettingsStorageCache::Callback* callback) {
  cache_.Clear();
  delegate_->Clear(
      CacheModifyingCallback::Create(
        callback,
        cache_ptr_factory_.GetWeakPtr()));
}

bool ExtensionSettingsStorageCache::GetFromCache(
    const std::string& key, Value** value) {
  Value* cached_value;
  if (!cache_.GetWithoutPathExpansion(key, &cached_value)) {
    return false;
  }
  *value = cached_value->DeepCopy();
  return true;
}
