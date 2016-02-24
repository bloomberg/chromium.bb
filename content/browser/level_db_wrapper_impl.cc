// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/level_db_wrapper_impl.h"

#include "base/bind.h"

namespace content {

LevelDBWrapperImpl::LevelDBWrapperImpl(
    const std::string& prefix, const base::Closure& no_bindings_callback)
    : prefix_(prefix), no_bindings_callback_(no_bindings_callback) {
  bindings_.set_connection_error_handler(base::Bind(
      &LevelDBWrapperImpl::OnConnectionError, base::Unretained(this)));
}

void LevelDBWrapperImpl::Bind(mojo::InterfaceRequest<LevelDBWrapper> request,
                              LevelDBObserverPtr observer) {
  // TODO(jam): store observer and call it when changes occur.
  bindings_.AddBinding(this, std::move(request));
}

LevelDBWrapperImpl::~LevelDBWrapperImpl() {
  no_bindings_callback_.Run();
}

void LevelDBWrapperImpl::Put(mojo::Array<uint8_t> key,
                             mojo::Array<uint8_t> value,
                             const mojo::String& source,
                             const PutCallback& callback) {
}

void LevelDBWrapperImpl::Delete(mojo::Array<uint8_t> key,
                                const mojo::String& source,
                                const DeleteCallback& callback) {
}

void LevelDBWrapperImpl::DeleteAll(const mojo::String& source,
                                   const DeleteAllCallback& callback) {
}

void LevelDBWrapperImpl::Get(mojo::Array<uint8_t> key,
                             const GetCallback& callback) {
}

void LevelDBWrapperImpl::GetAll(const GetAllCallback& callback) {
}

void LevelDBWrapperImpl::OnConnectionError() {
  if (!bindings_.empty())
    return;

  no_bindings_callback_.Run();
}

}  // namespace content
