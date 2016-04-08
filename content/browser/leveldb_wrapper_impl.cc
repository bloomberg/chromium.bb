// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/leveldb_wrapper_impl.h"

#include "base/bind.h"
#include "mojo/common/common_type_converters.h"

namespace content {

LevelDBWrapperImpl::LevelDBWrapperImpl(
    leveldb::LevelDBDatabase* database,
    const std::string& prefix,
    size_t max_size,
    const base::Closure& no_bindings_callback)
    : prefix_(prefix),
      no_bindings_callback_(no_bindings_callback),
      database_(database),
      bytes_used_(0),
      max_size_(max_size) {
  bindings_.set_connection_error_handler(base::Bind(
      &LevelDBWrapperImpl::OnConnectionError, base::Unretained(this)));
}

void LevelDBWrapperImpl::Bind(mojom::LevelDBWrapperRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void LevelDBWrapperImpl::AddObserver(mojom::LevelDBObserverPtr observer) {
  observers_.AddPtr(std::move(observer));
}

LevelDBWrapperImpl::~LevelDBWrapperImpl() {}

void LevelDBWrapperImpl::Put(mojo::Array<uint8_t> key,
                             mojo::Array<uint8_t> value,
                             const mojo::String& source,
                             const PutCallback& callback) {
  bool has_old_item = false;
  mojo::Array<uint8_t> old_value;
  size_t old_item_size = 0;
  auto found = map_.find(key);
  if (found != map_.end()) {
    old_value = std::move(found->second);
    old_item_size = key.size() + old_value.size();
    has_old_item = true;
  }
  size_t new_item_size = key.size() + value.size();
  size_t new_bytes_used = bytes_used_ - old_item_size + new_item_size;

  // Only check quota if the size is increasing, this allows
  // shrinking changes to pre-existing maps that are over budget.
  if (new_item_size > old_item_size && new_bytes_used > max_size_) {
    callback.Run(false);
    return;
  }

  map_[key.Clone()] = value.Clone();
  bytes_used_ = new_bytes_used;
  if (!has_old_item) {
    // We added a new key/value pair.
    observers_.ForAllPtrs(
        [&key, &value, &source](mojom::LevelDBObserver* observer) {
          observer->KeyAdded(key.Clone(), value.Clone(), source);
        });
  } else {
    // We changed the value for an existing key.
    observers_.ForAllPtrs(
        [&key, &value, &source, &old_value](mojom::LevelDBObserver* observer) {
          observer->KeyChanged(
              key.Clone(), value.Clone(), old_value.Clone(), source);
        });
  }
  callback.Run(true);
}

void LevelDBWrapperImpl::Delete(mojo::Array<uint8_t> key,
                                const mojo::String& source,
                                const DeleteCallback& callback) {
  auto found = map_.find(key);
  if (found == map_.end()) {
    callback.Run(true);
    return;
  }

  mojo::Array<uint8_t> old_value = std::move(found->second);
  map_.erase(found);
  bytes_used_ -= key.size() + old_value.size();
  observers_.ForAllPtrs(
      [&key, &source, &old_value](mojom::LevelDBObserver* observer) {
        observer->KeyDeleted(
            key.Clone(), old_value.Clone(), source);
      });
  callback.Run(true);
}

void LevelDBWrapperImpl::DeleteAll(const mojo::String& source,
                                   const DeleteAllCallback& callback) {
  map_.clear();
  bytes_used_ = 0;
  observers_.ForAllPtrs(
      [&source](mojom::LevelDBObserver* observer) {
        observer->AllDeleted(source);
      });
  callback.Run(true);
}

void LevelDBWrapperImpl::Get(mojo::Array<uint8_t> key,
                             const GetCallback& callback) {
  auto found = map_.find(key);
  if (found == map_.end()) {
    callback.Run(false, mojo::Array<uint8_t>());
    return;
  }
  callback.Run(true, found->second.Clone());
}

void LevelDBWrapperImpl::GetAll(const mojo::String& source,
                                const GetAllCallback& callback) {

  mojo::Array<mojom::KeyValuePtr> all(map_.size());
  for (const auto& it : map_) {
    mojom::KeyValuePtr kv = mojom::KeyValue::New();
    kv->key = it.first.Clone();
    kv->value = it.second.Clone();
    all.push_back(std::move(kv));
  }
  callback.Run(leveldb::DatabaseError::OK, std::move(all));
  observers_.ForAllPtrs(
      [source](mojom::LevelDBObserver* observer) {
        observer->GetAllComplete(source);
      });
}

void LevelDBWrapperImpl::OnConnectionError() {
  if (!bindings_.empty())
    return;

  no_bindings_callback_.Run();
}

}  // namespace content
