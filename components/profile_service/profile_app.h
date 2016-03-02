// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROFILE_SERVICE_PROFILE_APP_H_
#define COMPONENTS_PROFILE_SERVICE_PROFILE_APP_H_

#include "components/filesystem/lock_table.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "components/profile_service/public/interfaces/profile.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace filesystem {
class LockTable;
}

namespace profile {

scoped_ptr<mojo::ShellClient> CreateProfileApp();

// Application which hands off per-profile services.
//
// This Application serves ProfileService, and serves LevelDBService since most
// of the users of leveldb will want to write directly to the Directory
// provided by the ProfileService; we want file handling to be done in the same
// process to minimize IPC.
//
// In the future, this application will probably also offer any service that
// most Profile using applications will need, such as preferences.
class ProfileApp : public mojo::ShellClient,
                   public mojo::InterfaceFactory<ProfileService>,
                   public mojo::InterfaceFactory<leveldb::LevelDBService> {
 public:
  ProfileApp();
  ~ProfileApp() override;

  // Currently, ProfileApp is run from within the chrome process. This means it
  // that the ApplicationLoader is registered during MojoShellContext startup,
  // even though the application itself is not started. As soon as a
  // BrowserContext is created, the BrowserContext will choose a |user_id| for
  // itself and call us to register the mapping from |user_id| to
  // |profile_data_dir|.
  //
  // This data is then accessed when we get our Initialize() call.
  //
  // TODO(erg): This is a temporary hack until we redo how we initialize mojo
  // applications inside of chrome in general; this system won't work once
  // ProfileApp gets put in its own sandboxed process.
  static void AssociateMojoUserIDWithProfileDir(
      uint32_t user_id,
      const base::FilePath& profile_data_dir);

 private:
  // |ShellClient| override:
  void Initialize(mojo::Connector* connector,
                  const std::string& url,
                  uint32_t id,
                  uint32_t user_id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // |InterfaceFactory<ProfileService>| implementation:
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<ProfileService> request) override;

  // |InterfaceFactory<LevelDBService>| implementation:
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<leveldb::LevelDBService> request) override;

  mojo::TracingImpl tracing_;

  scoped_ptr<ProfileService> profile_service_;
  mojo::BindingSet<ProfileService> profile_bindings_;

  scoped_ptr<leveldb::LevelDBService> leveldb_service_;
  mojo::BindingSet<leveldb::LevelDBService> leveldb_bindings_;

  scoped_ptr<filesystem::LockTable> lock_table_;

  base::FilePath profile_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(ProfileApp);
};

}  // namespace profile

#endif  // COMPONENTS_PROFILE_SERVICE_PROFILE_APP_H_
