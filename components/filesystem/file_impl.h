// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILE_IMPL_H_
#define COMPONENTS_FILESYSTEM_FILE_IMPL_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace base {
class FilePath;
}

namespace filesystem {

class FileImpl : public File {
 public:
  FileImpl(mojo::InterfaceRequest<File> request,
           const base::FilePath& path,
           uint32_t flags);
  FileImpl(mojo::InterfaceRequest<File> request, base::File file);
  ~FileImpl() override;

  // |File| implementation:
  void Close(const CloseCallback& callback) override;
  void Read(uint32_t num_bytes_to_read,
            int64_t offset,
            Whence whence,
            const ReadCallback& callback) override;
  void ReadEntireFile(const ReadEntireFileCallback& callback) override;
  void Write(mojo::Array<uint8_t> bytes_to_write,
             int64_t offset,
             Whence whence,
             const WriteCallback& callback) override;
  void Tell(const TellCallback& callback) override;
  void Seek(int64_t offset,
            Whence whence,
            const SeekCallback& callback) override;
  void Stat(const StatCallback& callback) override;
  void Truncate(int64_t size, const TruncateCallback& callback) override;
  void Touch(TimespecOrNowPtr atime,
             TimespecOrNowPtr mtime,
             const TouchCallback& callback) override;
  void Dup(mojo::InterfaceRequest<File> file,
           const DupCallback& callback) override;
  void Flush(const FlushCallback& callback) override;
  void AsHandle(const AsHandleCallback& callback) override;

 private:
  mojo::StrongBinding<File> binding_;
  base::File file_;

  DISALLOW_COPY_AND_ASSIGN(FileImpl);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILE_IMPL_H_
