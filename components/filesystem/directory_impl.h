// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_FILES_DIRECTORY_IMPL_H_
#define SERVICES_FILES_DIRECTORY_IMPL_H_

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace base {
class ScopedTempDir;
}  // namespace base

namespace mojo {
namespace files {

class DirectoryImpl : public Directory {
 public:
  // Set |temp_dir| only if there's a temporary directory that should be deleted
  // when this object is destroyed.
  DirectoryImpl(InterfaceRequest<Directory> request,
                base::ScopedFD dir_fd,
                scoped_ptr<base::ScopedTempDir> temp_dir);
  ~DirectoryImpl() override;

  // |Directory| implementation:
  void Read(const ReadCallback& callback) override;
  void Stat(const StatCallback& callback) override;
  void Touch(TimespecOrNowPtr atime,
             TimespecOrNowPtr mtime,
             const TouchCallback& callback) override;
  void OpenFile(const String& path,
                InterfaceRequest<File> file,
                uint32_t open_flags,
                const OpenFileCallback& callback) override;
  void OpenDirectory(const String& path,
                     InterfaceRequest<Directory> directory,
                     uint32_t open_flags,
                     const OpenDirectoryCallback& callback) override;
  void Rename(const String& path,
              const String& new_path,
              const RenameCallback& callback) override;
  void Delete(const String& path,
              uint32_t delete_flags,
              const DeleteCallback& callback) override;

 private:
  StrongBinding<Directory> binding_;
  base::ScopedFD dir_fd_;
  scoped_ptr<base::ScopedTempDir> temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryImpl);
};

}  // namespace files
}  // namespace mojo

#endif  // SERVICES_FILES_DIRECTORY_IMPL_H_
