// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/profile_service/profile_app.h"

#include "base/lazy_instance.h"
#include "components/leveldb/leveldb_service_impl.h"
#include "components/profile_service/profile_service_impl.h"
#include "mojo/shell/public/cpp/connection.h"

namespace profile {

namespace {

base::LazyInstance<std::map<std::string, base::FilePath>>
    g_user_id_to_data_dir = LAZY_INSTANCE_INITIALIZER;

}  // namespace

scoped_ptr<mojo::ShellClient> CreateProfileApp() {
  return make_scoped_ptr(new ProfileApp);
}

ProfileApp::ProfileApp()
    : lock_table_(new filesystem::LockTable) {
}

ProfileApp::~ProfileApp() {}

// static
void ProfileApp::AssociateMojoUserIDWithProfileDir(
    const std::string& user_id,
    const base::FilePath& profile_data_dir) {
  g_user_id_to_data_dir.Get()[user_id] = profile_data_dir;
}

void ProfileApp::Initialize(mojo::Connector* connector,
                            const std::string& url,
                            const std::string& user_id,
                            uint32_t id) {
  tracing_.Initialize(connector, url);
  leveldb_service_.reset(new leveldb::LevelDBServiceImpl);

  auto it = g_user_id_to_data_dir.Get().find(user_id);
  DCHECK(it != g_user_id_to_data_dir.Get().end());
  profile_data_dir_ = it->second;
}

bool ProfileApp::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<leveldb::LevelDBService>(this);
  connection->AddInterface<ProfileService>(this);
  return true;
}

void ProfileApp::Create(mojo::Connection* connection,
                        ProfileServiceRequest request) {
  // No, we need one of these per connection.
  new ProfileServiceImpl(connection,
                         std::move(request),
                         profile_data_dir_,
                         lock_table_.get());
}

void ProfileApp::Create(mojo::Connection* connection,
                        leveldb::LevelDBServiceRequest request) {
  leveldb_bindings_.AddBinding(leveldb_service_.get(), std::move(request));
}

}  // namespace profile
