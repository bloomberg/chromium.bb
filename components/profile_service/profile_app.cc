// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/profile_service/profile_app.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/filesystem/lock_table.h"
#include "components/leveldb/leveldb_service_impl.h"
#include "components/profile_service/profile_service_impl.h"
#include "components/profile_service/user_id_map.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/shell/public/cpp/connection.h"

namespace profile {

class ProfileApp::ProfileServiceObjects
    : public base::SupportsWeakPtr<ProfileServiceObjects> {
 public:
  // Created on the main thread.
  ProfileServiceObjects(base::FilePath profile_data_dir)
      : profile_data_dir_(profile_data_dir) {}

  // Destroyed on the |profile_service_runner_|.
  ~ProfileServiceObjects() {}

  // Called on the |profile_service_runner_|.
  void OnProfileServiceRequest(mojo::Connection* connection,
                               ProfileServiceRequest request) {
    if (!lock_table_)
      lock_table_ = new filesystem::LockTable;
    profile_service_bindings_.AddBinding(
        new ProfileServiceImpl(profile_data_dir_, lock_table_),
        std::move(request));
  }

 private:
  mojo::BindingSet<ProfileService> profile_service_bindings_;
  scoped_refptr<filesystem::LockTable> lock_table_;
  base::FilePath profile_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(ProfileServiceObjects);
};

class ProfileApp::LevelDBServiceObjects
    : public base::SupportsWeakPtr<LevelDBServiceObjects> {
 public:
  // Created on the main thread.
  LevelDBServiceObjects(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}

  // Destroyed on the |leveldb_service_runner_|.
  ~LevelDBServiceObjects() {}

  // Called on the |leveldb_service_runner_|.
  void OnLevelDBServiceRequest(mojo::Connection* connection,
                               leveldb::LevelDBServiceRequest request) {
    if (!leveldb_service_)
      leveldb_service_.reset(new leveldb::LevelDBServiceImpl(task_runner_));
    leveldb_bindings_.AddBinding(leveldb_service_.get(), std::move(request));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Variables that are only accessible on the |leveldb_service_runner_| thread.
  scoped_ptr<leveldb::LevelDBService> leveldb_service_;
  mojo::BindingSet<leveldb::LevelDBService> leveldb_bindings_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBServiceObjects);
};

scoped_ptr<mojo::ShellClient> CreateProfileApp(
    scoped_refptr<base::SingleThreadTaskRunner> profile_service_runner,
    scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner) {
  return make_scoped_ptr(new ProfileApp(
      std::move(profile_service_runner),
      std::move(leveldb_service_runner)));
}

ProfileApp::ProfileApp(
    scoped_refptr<base::SingleThreadTaskRunner> profile_service_runner,
    scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner)
    : profile_service_runner_(std::move(profile_service_runner)),
      leveldb_service_runner_(std::move(leveldb_service_runner)) {}

ProfileApp::~ProfileApp() {
  profile_service_runner_->DeleteSoon(FROM_HERE, profile_objects_.release());
  leveldb_service_runner_->DeleteSoon(FROM_HERE, leveldb_objects_.release());
}

void ProfileApp::Initialize(mojo::Connector* connector,
                            const mojo::Identity& identity,
                            uint32_t id) {
  tracing_.Initialize(connector, identity.name());
  profile_objects_.reset(new ProfileApp::ProfileServiceObjects(
      GetProfileDirForUserID(identity.user_id())));
  leveldb_objects_.reset(
      new ProfileApp::LevelDBServiceObjects(leveldb_service_runner_));
}

bool ProfileApp::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<leveldb::LevelDBService>(this);
  connection->AddInterface<ProfileService>(this);
  return true;
}

void ProfileApp::Create(mojo::Connection* connection,
                        ProfileServiceRequest request) {
  profile_service_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ProfileApp::ProfileServiceObjects::OnProfileServiceRequest,
                 profile_objects_->AsWeakPtr(), connection,
                 base::Passed(&request)));
}

void ProfileApp::Create(mojo::Connection* connection,
                        leveldb::LevelDBServiceRequest request) {
  leveldb_service_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ProfileApp::LevelDBServiceObjects::OnLevelDBServiceRequest,
                 leveldb_objects_->AsWeakPtr(), connection,
                 base::Passed(&request)));
}

}  // namespace profile
