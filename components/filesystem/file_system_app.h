// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_
#define COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_

#include "base/macros.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/file_system_impl.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace mojo {
class ApplicationImpl;
}

namespace filesystem {

class FileSystemApp : public mojo::ApplicationDelegate,
                      public mojo::InterfaceFactory<FileSystem> {
 public:
  FileSystemApp();
  ~FileSystemApp() override;

  // Called by individual FileSystem objects to register lifetime events.
  void RegisterDirectoryToClient(DirectoryImpl* directory,
                                 FileSystemClientPtr client);

 private:
  // We set the DirectoryImpl's error handler to this function. We do this so
  // that we can QuitNow() once the last DirectoryImpl has closed itself.
  void OnDirectoryConnectionError(DirectoryImpl* directory);

  // |ApplicationDelegate| override:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;
  bool ShellConnectionLost() override;

  // |InterfaceFactory<Files>| implementation:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<FileSystem> request) override;

  // Use a vector to work around map not letting us use FileSystemClientPtr as
  // a value in a std::map. The move constructors are to allow us to deal with
  // FileSystemClientPtr inside a vector.
  struct Client {
    Client(DirectoryImpl* directory, FileSystemClientPtr fs_client);
    Client(Client&& rhs);
    ~Client();

    Client& operator=(Client&& rhs);

    DirectoryImpl* directory_;
    FileSystemClientPtr fs_client_;
  };
  std::vector<Client> client_mapping_;

  mojo::ApplicationImpl* app_;
  mojo::TracingImpl tracing_;

  // Set to true when our shell connection is closed. On connection error, we
  // then broadcast a notification to all FileSystemClients that they should
  // shut down. Once the final one does, then we QuitNow().
  bool in_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemApp);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_
