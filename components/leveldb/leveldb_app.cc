// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_app.h"

#include "base/message_loop/message_loop.h"
#include "components/leveldb/leveldb_service_impl.h"
#include "mojo/shell/public/cpp/connection.h"

namespace leveldb {

LevelDBApp::LevelDBApp() {}

LevelDBApp::~LevelDBApp() {}

void LevelDBApp::Initialize(mojo::Connector* connector,
                            const mojo::Identity& identity,
                            uint32_t id) {
  tracing_.Initialize(connector, identity.name());
}

bool LevelDBApp::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<LevelDBService>(this);
  return true;
}

void LevelDBApp::Create(mojo::Connection* connection,
                        leveldb::LevelDBServiceRequest request) {
  if (!service_)
    service_.reset(new LevelDBServiceImpl);
  bindings_.AddBinding(service_.get(), std::move(request));
}

}  // namespace leveldb
