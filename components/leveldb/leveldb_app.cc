// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_app.h"

#include "base/message_loop/message_loop.h"
#include "components/leveldb/leveldb_service_impl.h"
#include "services/shell/public/cpp/interface_registry.h"

namespace leveldb {

LevelDBApp::LevelDBApp() {}

LevelDBApp::~LevelDBApp() {}

void LevelDBApp::OnStart(const shell::Identity& identity) {
  tracing_.Initialize(connector(), identity.name());
}

bool LevelDBApp::OnConnect(const shell::Identity& remote_identity,
                           shell::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::LevelDBService>(this);
  return true;
}

void LevelDBApp::Create(const shell::Identity& remote_identity,
                        leveldb::mojom::LevelDBServiceRequest request) {
  if (!service_)
    service_.reset(
        new LevelDBServiceImpl(base::MessageLoop::current()->task_runner()));
  bindings_.AddBinding(service_.get(), std::move(request));
}

}  // namespace leveldb
