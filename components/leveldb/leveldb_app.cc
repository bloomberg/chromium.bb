// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_app.h"

#include "components/leveldb/leveldb_service_impl.h"
#include "mojo/shell/public/cpp/connection.h"

namespace leveldb {

LevelDBApp::LevelDBApp() {}

LevelDBApp::~LevelDBApp() {}

void LevelDBApp::Initialize(mojo::Connector* connector,
                            const std::string& url,
                            const std::string& user_id,
                            uint32_t id) {
  tracing_.Initialize(connector, url);
  service_.reset(new LevelDBServiceImpl);
}

bool LevelDBApp::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<LevelDBService>(this);
  return true;
}

void LevelDBApp::Create(mojo::Connection* connection,
                        mojo::InterfaceRequest<LevelDBService> request) {
  bindings_.AddBinding(service_.get(), std::move(request));
}

}  // namespace leveldb
