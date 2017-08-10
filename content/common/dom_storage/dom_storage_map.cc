// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/dom_storage/dom_storage_map.h"

#include "base/logging.h"

namespace content {

namespace {

size_t size_in_memory(const base::string16& key, const size_t value) {
  return key.length() * sizeof(base::char16) + sizeof(size_t);
}

size_t size_in_memory(const base::string16& key,
                      const base::NullableString16& value) {
  return (key.length() + value.string().length()) * sizeof(base::char16);
}

size_t size_of_item(const base::string16& key, const size_t value) {
  return key.length() * sizeof(base::char16) + value;
}

size_t size_of_item(const base::string16& key,
                    const base::NullableString16& value) {
  // Null value indicates deletion. So, key size is not counted.
  return value.is_null() ? 0 : size_in_memory(key, value);
}

}  // namespace

DOMStorageMap::DOMStorageMap(size_t quota) : DOMStorageMap(quota, false) {}

DOMStorageMap::DOMStorageMap(size_t quota, bool has_only_keys)
    : bytes_used_(0),
      memory_usage_(0),
      quota_(quota),
      has_only_keys_(has_only_keys) {
  ResetKeyIterator();
}

DOMStorageMap::~DOMStorageMap() {}

unsigned DOMStorageMap::Length() const {
  return has_only_keys_ ? keys_only_.size() : keys_values_.size();
}

base::NullableString16 DOMStorageMap::Key(unsigned index) {
  if (index >= Length())
    return base::NullableString16();
  while (last_key_index_ != index) {
    if (last_key_index_ > index) {
      if (has_only_keys_)
        --keys_only_iterator_;
      else
        --keys_values_iterator_;
      --last_key_index_;
    } else {
      if (has_only_keys_)
        ++keys_only_iterator_;
      else
        ++keys_values_iterator_;
      ++last_key_index_;
    }
  }
  return base::NullableString16(has_only_keys_ ? keys_only_iterator_->first
                                               : keys_values_iterator_->first,
                                false);
}

bool DOMStorageMap::SetItem(
    const base::string16& key, const base::string16& value,
    base::NullableString16* old_value) {
  if (has_only_keys_) {
    size_t unused;
    size_t value_size = value.length() * sizeof(base::char16);
    return SetItemInternal<KeysMap>(&keys_only_, key, value_size, &unused);
  } else {
    base::NullableString16 new_value(value, false);
    *old_value = base::NullableString16();
    return SetItemInternal<DOMStorageValuesMap>(&keys_values_, key, new_value,
                                                old_value);
  }
}

bool DOMStorageMap::RemoveItem(
    const base::string16& key,
    base::string16* old_value) {
  if (has_only_keys_) {
    size_t unused;
    return RemoveItemInternal<KeysMap>(&keys_only_, key, &unused);
  } else {
    base::NullableString16 nullable_old;
    bool success = RemoveItemInternal<DOMStorageValuesMap>(&keys_values_, key,
                                                           &nullable_old);
    if (success)
      *old_value = nullable_old.string();
    return success;
  }
}

base::NullableString16 DOMStorageMap::GetItem(const base::string16& key) const {
  DCHECK(!has_only_keys_);
  DOMStorageValuesMap::const_iterator found = keys_values_.find(key);
  if (found == keys_values_.end())
    return base::NullableString16();
  return found->second;
}

void DOMStorageMap::ExtractValues(DOMStorageValuesMap* map) const {
  DCHECK(!has_only_keys_);
  *map = keys_values_;
}

void DOMStorageMap::SwapValues(DOMStorageValuesMap* values) {
  // Note: A pre-existing file may be over the quota budget.
  DCHECK(!has_only_keys_);
  keys_values_.swap(*values);
  bytes_used_ = CountBytes(keys_values_);
  ResetKeyIterator();
}

void DOMStorageMap::TakeKeysFrom(const DOMStorageValuesMap* values) {
  // Note: A pre-existing file may be over the quota budget.
  DCHECK(has_only_keys_);
  keys_only_.clear();
  memory_usage_ = 0;
  for (const auto& item : *values) {
    keys_only_[item.first] =
        item.second.string().length() * sizeof(base::char16);
    // Do not count size of values for memory usage.
    memory_usage_ += size_in_memory(item.first, 0 /* unused */);
  }
  bytes_used_ = CountBytes(*values);
  ResetKeyIterator();
}

DOMStorageMap* DOMStorageMap::DeepCopy() const {
  DOMStorageMap* copy = new DOMStorageMap(quota_, has_only_keys_);
  copy->keys_values_ = keys_values_;
  copy->keys_only_ = keys_only_;
  copy->bytes_used_ = bytes_used_;
  copy->memory_usage_ = memory_usage_;
  copy->ResetKeyIterator();
  return copy;
}

void DOMStorageMap::ResetKeyIterator() {
  keys_only_iterator_ = keys_only_.begin();
  keys_values_iterator_ = keys_values_.begin();
  last_key_index_ = 0;
}

// static
size_t DOMStorageMap::CountBytes(const DOMStorageValuesMap& values) {
  if (values.empty())
    return 0;

  size_t count = 0;
  for (const auto& pair : values)
    count += size_of_item(pair.first, pair.second);
  return count;
}

template <typename MapType>
bool DOMStorageMap::SetItemInternal(MapType* map_type,
                                    const base::string16& key,
                                    typename MapType::mapped_type value,
                                    typename MapType::mapped_type* old_value) {
  const auto found = map_type->find(key);
  size_t old_item_size = 0;
  size_t old_item_memory = 0;
  if (found != map_type->end()) {
    *old_value = found->second;
    old_item_size = size_of_item(key, *old_value);
    old_item_memory = size_in_memory(key, *old_value);
  }
  size_t new_item_size = size_of_item(key, value);
  size_t new_bytes_used = bytes_used_ - old_item_size + new_item_size;

  // Only check quota if the size is increasing, this allows
  // shrinking changes to pre-existing files that are over budget.
  if (new_item_size > old_item_size && new_bytes_used > quota_)
    return false;

  (*map_type)[key] = value;
  ResetKeyIterator();
  bytes_used_ = new_bytes_used;
  memory_usage_ = memory_usage_ + size_in_memory(key, value) - old_item_memory;
  return true;
}

template <typename MapType>
bool DOMStorageMap::RemoveItemInternal(
    MapType* map_type,
    const base::string16& key,
    typename MapType::mapped_type* old_value) {
  const auto found = map_type->find(key);
  if (found == map_type->end())
    return false;
  *old_value = found->second;
  map_type->erase(found);
  ResetKeyIterator();
  bytes_used_ -= size_of_item(key, *old_value);
  memory_usage_ -= size_in_memory(key, *old_value);
  return true;
}

}  // namespace content
