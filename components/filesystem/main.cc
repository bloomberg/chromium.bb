// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/filesystem/file_system_impl.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/public/c/system/main.h"

namespace filesystem {

class FilesApp : public mojo::ApplicationDelegate,
                 public mojo::InterfaceFactory<FileSystem> {
 public:
  FilesApp() {}
  ~FilesApp() override {}

 private:
  // |ApplicationDelegate| override:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<FileSystem>(this);
    return true;
  }

  // |InterfaceFactory<Files>| implementation:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<FileSystem> request) override {
    new FileSystemImpl(connection, request.Pass());
  }

  DISALLOW_COPY_AND_ASSIGN(FilesApp);
};

}  // namespace filesystem

MojoResult MojoMain(MojoHandle application_request) {
  mojo::ApplicationRunner runner(new filesystem::FilesApp());
  return runner.Run(application_request);
}
