// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_LEVELDB_APP_H_
#define COMPONENTS_LEVELDB_LEVELDB_APP_H_

#include "components/leveldb/leveldb_file_thread.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace leveldb {

class LevelDBApp : public mojo::ShellClient,
                   public mojo::InterfaceFactory<LevelDBService>,
                   public LevelDBService {
 public:
  LevelDBApp();
  ~LevelDBApp() override;

 private:
  // |ShellClient| override:
  void Initialize(mojo::Shell* shell,
                  const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // TODO(erg): What do we have to do on shell error?
  // bool OnShellConnectionError() override;

  // |InterfaceFactory<LevelDBService>| implementation:
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<LevelDBService> request) override;

  // Overridden from LevelDBService:
  void Open(filesystem::DirectoryPtr directory,
            const mojo::String& dbname,
            mojo::InterfaceRequest<LevelDBDatabase> database,
            const OpenCallback& callback) override;

  mojo::TracingImpl tracing_;
  mojo::WeakBindingSet<LevelDBService> bindings_;

  // Thread to own the mojo message pipe. Because leveldb spawns multiple
  // threads that want to call file stuff, we create a dedicated thread to send
  // and receive mojo message calls.
  scoped_refptr<LevelDBFileThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBApp);
};

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_LEVELDB_APP_H_
