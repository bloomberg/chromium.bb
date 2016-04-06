// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LEVELDB_WRAPPER_IMPL_H_
#define CONTENT_BROWSER_LEVELDB_WRAPPER_IMPL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/leveldb_wrapper.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace content {

// This is a wrapper around a leveldb::LevelDBDatabase. It adds a couple of
// features:
// 1) Adds the given prefix, if any, to all keys. This allows the sharing of one
//    database across many, possibly untrusted, consumers and ensuring that they
//    can't access each other's values.
// 2) Enforces a max_size constraint.
// 3) Informs an observer when the given prefix' values are modified.
// 4) Throttles requests to avoid overwhelming the disk.
class LevelDBWrapperImpl : public mojom::LevelDBWrapper {
 public:
  // |no_bindings_callback| will be called when this object has no more
  // bindings.
  LevelDBWrapperImpl(leveldb::LevelDBDatabase* database,
                     const std::string& prefix,
                     size_t max_size,
                     const base::Closure& no_bindings_callback);
  ~LevelDBWrapperImpl() override;

  void Bind(mojom::LevelDBWrapperRequest request);
  void AddObserver(mojom::LevelDBObserverPtr observer);

 private:
  // LevelDBWrapperImpl:
  void Put(mojo::Array<uint8_t> key,
           mojo::Array<uint8_t> value,
           const mojo::String& source,
           const PutCallback& callback) override;
  void Delete(mojo::Array<uint8_t> key,
              const mojo::String& source,
              const DeleteCallback& callback) override;
  void DeleteAll(const mojo::String& source,
                 const DeleteAllCallback& callback) override;
  void Get(mojo::Array<uint8_t> key, const GetCallback& callback) override;
  void GetAll(const mojo::String& source,
              const GetAllCallback& callback) override;

  void OnConnectionError();

  std::string prefix_;
  mojo::BindingSet<mojom::LevelDBWrapper> bindings_;
  mojo::InterfacePtrSet<mojom::LevelDBObserver> observers_;
  base::Closure no_bindings_callback_;
  leveldb::LevelDBDatabase* database_;
  std::map<mojo::Array<uint8_t>, mojo::Array<uint8_t>> map_;
  size_t bytes_used_;
  size_t max_size_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBWrapperImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LEVELDB_WRAPPER_IMPL_H_
