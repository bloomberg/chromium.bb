// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_
#define COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_

#include "base/macros.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/file_system_impl.h"
#include "components/filesystem/lock_table.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace mojo {
class Connector;
}

namespace filesystem {

class FileSystemApp : public mojo::ShellClient,
                      public mojo::InterfaceFactory<FileSystem> {
 public:
  FileSystemApp();
  ~FileSystemApp() override;

 private:
  // Gets the system specific toplevel profile directory.
  static base::FilePath GetUserDataDir();

  // |mojo::ShellClient| override:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;
  // |InterfaceFactory<Files>| implementation:
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<FileSystem> request) override;

  mojo::TracingImpl tracing_;

  scoped_refptr<LockTable> lock_table_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemApp);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_
