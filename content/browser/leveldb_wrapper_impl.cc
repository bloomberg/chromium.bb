// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/leveldb_wrapper_impl.h"

#include "base/bind.h"

namespace content {

LevelDBWrapperImpl::LevelDBWrapperImpl(
    leveldb::LevelDBDatabase* database,
    const std::string& prefix,
    const base::Closure& no_bindings_callback)
    : prefix_(prefix),
      no_bindings_callback_(no_bindings_callback),
      database_(database) {
  bindings_.set_connection_error_handler(base::Bind(
      &LevelDBWrapperImpl::OnConnectionError, base::Unretained(this)));
}

void LevelDBWrapperImpl::Bind(LevelDBWrapperRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

LevelDBWrapperImpl::~LevelDBWrapperImpl() {}

void LevelDBWrapperImpl::Put(mojo::Array<uint8_t> key,
                             mojo::Array<uint8_t> value,
                             const mojo::String& source,
                             const PutCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(leveldb::DatabaseError::NOT_SUPPORTED);
}

void LevelDBWrapperImpl::Delete(mojo::Array<uint8_t> key,
                                const mojo::String& source,
                                const DeleteCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(leveldb::DatabaseError::NOT_SUPPORTED);
}

void LevelDBWrapperImpl::DeleteAll(LevelDBObserverPtr observer,
                                   const mojo::String& source,
                                   const DeleteAllCallback& callback) {
  // TODO(jam): store observer and call it when changes occur.
  NOTIMPLEMENTED();
  callback.Run(leveldb::DatabaseError::NOT_SUPPORTED);
}

void LevelDBWrapperImpl::Get(mojo::Array<uint8_t> key,
                             const GetCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(leveldb::DatabaseError::NOT_SUPPORTED, mojo::Array<uint8_t>());
}

void LevelDBWrapperImpl::GetAll(LevelDBObserverPtr observer,
                                const GetAllCallback& callback) {
  // TODO(jam): store observer and call it when changes occur.
  NOTIMPLEMENTED();
  callback.Run(leveldb::DatabaseError::NOT_SUPPORTED,
               mojo::Array<KeyValuePtr>());
}

void LevelDBWrapperImpl::OnConnectionError() {
  if (!bindings_.empty())
    return;

  no_bindings_callback_.Run();
}

}  // namespace content
