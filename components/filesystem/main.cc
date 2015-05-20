// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/filesystem/files_impl.h"
#include "components/filesystem/public/interfaces/files.mojom.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/public/c/system/main.h"

namespace mojo {
namespace files {

class FilesApp : public ApplicationDelegate, public InterfaceFactory<Files> {
 public:
  FilesApp() {}
  ~FilesApp() override {}

 private:
  // |ApplicationDelegate| override:
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService<Files>(this);
    return true;
  }

  // |InterfaceFactory<Files>| implementation:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<Files> request) override {
    new FilesImpl(connection, request.Pass());
  }

  DISALLOW_COPY_AND_ASSIGN(FilesApp);
};

}  // namespace files
}  // namespace mojo

MojoResult MojoMain(MojoHandle application_request) {
  mojo::ApplicationRunner runner(new mojo::files::FilesApp());
  return runner.Run(application_request);
}
