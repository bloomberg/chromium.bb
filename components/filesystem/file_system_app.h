// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_
#define COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_

#include "base/macros.h"
#include "components/filesystem/file_system_impl.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"

namespace filesystem {

class FileSystemApp : public mojo::ApplicationDelegate,
                      public mojo::InterfaceFactory<FileSystem> {
 public:
  FileSystemApp();
  ~FileSystemApp() override;

 private:
  // |ApplicationDelegate| override:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // |InterfaceFactory<Files>| implementation:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<FileSystem> request) override;

  DISALLOW_COPY_AND_ASSIGN(FileSystemApp);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_
