// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILE_SYSTEM_IMPL_H_
#define COMPONENTS_FILESYSTEM_FILE_SYSTEM_IMPL_H_

#include "base/macros.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mojo {
class ApplicationConnection;
}

namespace filesystem {

class FileSystemImpl : public FileSystem {
 public:
  FileSystemImpl(mojo::ApplicationConnection* connection,
                 mojo::InterfaceRequest<FileSystem> request);
  ~FileSystemImpl() override;

  // |Files| implementation:
  // We provide a "private" temporary file system as the default. In Debug
  // builds, we also provide access to a common file system named "debug"
  // (stored under ~/MojoDebug).
  void OpenFileSystem(const mojo::String& file_system,
                      mojo::InterfaceRequest<Directory> directory,
                      const OpenFileSystemCallback& callback) override;

 private:
  const std::string remote_application_url_;
  mojo::StrongBinding<FileSystem> binding_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemImpl);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILE_SYSTEM_IMPL_H_
