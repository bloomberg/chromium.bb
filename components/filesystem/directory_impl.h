// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_DIRECTORY_IMPL_H_
#define COMPONENTS_FILESYSTEM_DIRECTORY_IMPL_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace base {
class ScopedTempDir;
}  // namespace base

namespace filesystem {

class DirectoryImpl : public Directory {
 public:
  // Set |temp_dir| only if there's a temporary directory that should be deleted
  // when this object is destroyed.
  DirectoryImpl(mojo::InterfaceRequest<Directory> request,
                base::FilePath directory_path,
                scoped_ptr<base::ScopedTempDir> temp_dir);
  ~DirectoryImpl() override;

  void set_connection_error_handler(const mojo::Closure& error_handler) {
    binding_.set_connection_error_handler(error_handler);
  }

  // |Directory| implementation:
  void Read(const ReadCallback& callback) override;
  void OpenFile(const mojo::String& path,
                mojo::InterfaceRequest<File> file,
                uint32_t open_flags,
                const OpenFileCallback& callback) override;
  void OpenDirectory(const mojo::String& path,
                     mojo::InterfaceRequest<Directory> directory,
                     uint32_t open_flags,
                     const OpenDirectoryCallback& callback) override;
  void Rename(const mojo::String& path,
              const mojo::String& new_path,
              const RenameCallback& callback) override;
  void Delete(const mojo::String& path,
              uint32_t delete_flags,
              const DeleteCallback& callback) override;
  void Exists(const mojo::String& path,
              const ExistsCallback& callback) override;
  void IsWritable(const mojo::String& path,
                  const IsWritableCallback& callback) override;
  void Flush(const FlushCallback& callback) override;

 private:
  mojo::StrongBinding<Directory> binding_;
  base::FilePath directory_path_;
  scoped_ptr<base::ScopedTempDir> temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryImpl);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_DIRECTORY_IMPL_H_
