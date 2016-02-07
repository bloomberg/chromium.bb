// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILE_SYSTEM_IMPL_H_
#define COMPONENTS_FILESYSTEM_FILE_SYSTEM_IMPL_H_

#include "base/macros.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace base {
class FilePath;
}

namespace mojo {
class Connection;
}

namespace filesystem {
class FileSystemApp;

class FileSystemImpl : public FileSystem {
 public:
  FileSystemImpl(FileSystemApp* app,
                 mojo::Connection* connection,
                 mojo::InterfaceRequest<FileSystem> request);
  ~FileSystemImpl() override;

  // |Files| implementation:

  // Current valid values for |file_system| are "temp" for a temporary
  // filesystem and "origin" for a persistent filesystem bound to the origin of
  // the URL of the caller.
  void OpenFileSystem(const mojo::String& file_system,
                      mojo::InterfaceRequest<Directory> directory,
                      FileSystemClientPtr client,
                      const OpenFileSystemCallback& callback) override;

 private:
  // Gets the system specific toplevel profile directory.
  base::FilePath GetSystemProfileDir() const;

  // Takes the origin string from |remote_application_url_|.
  std::string GetOriginFromRemoteApplicationURL() const;

  // Sanitizes |origin| so it is an acceptable filesystem name.
  void BuildSanitizedOrigin(const std::string& origin,
                            std::string* sanitized_origin);

  FileSystemApp* app_;
  const std::string remote_application_url_;
  mojo::StrongBinding<FileSystem> binding_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemImpl);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILE_SYSTEM_IMPL_H_
