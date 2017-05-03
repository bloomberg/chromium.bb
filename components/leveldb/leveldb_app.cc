// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_app.h"

#include "components/leveldb/leveldb_service_impl.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace leveldb {

LevelDBApp::LevelDBApp() : file_thread_("LevelDBFile") {
  registry_.AddInterface<mojom::LevelDBService>(
      base::Bind(&LevelDBApp::Create, base::Unretained(this)));
}

LevelDBApp::~LevelDBApp() {}

void LevelDBApp::OnStart() {}

void LevelDBApp::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(source_info, interface_name,
                          std::move(interface_pipe));
}

void LevelDBApp::Create(const service_manager::BindSourceInfo& source_info,
                        leveldb::mojom::LevelDBServiceRequest request) {
  if (!service_) {
    if (!file_thread_.IsRunning())
      file_thread_.Start();
    service_.reset(
        new LevelDBServiceImpl(file_thread_.message_loop()->task_runner()));
  }
  bindings_.AddBinding(service_.get(), std::move(request));
}

}  // namespace leveldb
