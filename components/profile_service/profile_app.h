// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROFILE_SERVICE_PROFILE_APP_H_
#define COMPONENTS_PROFILE_SERVICE_PROFILE_APP_H_

#include "base/memory/ref_counted.h"
#include "components/filesystem/lock_table.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "components/profile_service/public/interfaces/profile.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace profile {

scoped_ptr<mojo::ShellClient> CreateProfileApp(
    scoped_refptr<base::SingleThreadTaskRunner> profile_service_runner,
    scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner);

// Application which hands off per-profile services.
//
// This Application serves ProfileService. In the future, this application will
// probably also offer any service that most Profile using applications will
// need, such as preferences; this class will have to be made into a
// application which is an InterfaceProvider which internally spawns threads
// for different sub-applications.
class ProfileApp : public mojo::ShellClient,
                   public mojo::InterfaceFactory<ProfileService>,
                   public mojo::InterfaceFactory<leveldb::LevelDBService> {
 public:
  ProfileApp(
      scoped_refptr<base::SingleThreadTaskRunner> profile_service_runner,
      scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner);
  ~ProfileApp() override;

 private:
  // |ShellClient| override:
  void Initialize(mojo::Connector* connector,
                  const mojo::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // |InterfaceFactory<ProfileService>| implementation:
  void Create(mojo::Connection* connection,
              ProfileServiceRequest request) override;

  // |InterfaceFactory<LevelDBService>| implementation:
  void Create(mojo::Connection* connection,
              leveldb::LevelDBServiceRequest request) override;

  void OnLevelDBServiceRequest(mojo::Connection* connection,
                               leveldb::LevelDBServiceRequest request);
  void OnLevelDBServiceError();

  scoped_refptr<base::SingleThreadTaskRunner> profile_service_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> leveldb_service_runner_;

  mojo::TracingImpl tracing_;

  // We create these two objects so we can delete them on the correct task
  // runners.
  class ProfileServiceObjects;
  scoped_ptr<ProfileServiceObjects> profile_objects_;

  class LevelDBServiceObjects;
  scoped_ptr<LevelDBServiceObjects> leveldb_objects_;

  DISALLOW_COPY_AND_ASSIGN(ProfileApp);
};

}  // namespace profile

#endif  // COMPONENTS_PROFILE_SERVICE_PROFILE_APP_H_
