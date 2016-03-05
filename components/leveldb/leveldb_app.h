// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_LEVELDB_APP_H_
#define COMPONENTS_LEVELDB_LEVELDB_APP_H_

#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace leveldb {

class LevelDBApp : public mojo::ShellClient,
                   public mojo::InterfaceFactory<LevelDBService> {
 public:
  LevelDBApp();
  ~LevelDBApp() override;

 private:
  // |ShellClient| override:
  void Initialize(mojo::Connector* connector,
                  const std::string& url,
                  const std::string& user_id,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // TODO(erg): What do we have to do on shell error?
  // bool OnShellConnectionError() override;

  // |InterfaceFactory<LevelDBService>| implementation:
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<LevelDBService> request) override;

  mojo::TracingImpl tracing_;
  scoped_ptr<LevelDBService> service_;
  mojo::BindingSet<LevelDBService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBApp);
};

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_LEVELDB_APP_H_
