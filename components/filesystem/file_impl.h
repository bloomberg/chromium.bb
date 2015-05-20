// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_FILES_FILE_IMPL_H_
#define SERVICES_FILES_FILE_IMPL_H_

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mojo {
namespace files {

class FileImpl : public File {
 public:
  // TODO(vtl): Will need more for, e.g., |Reopen()|.
  FileImpl(InterfaceRequest<File> request, base::ScopedFD file_fd);
  ~FileImpl() override;

  // |File| implementation:
  void Close(const CloseCallback& callback) override;
  void Read(uint32_t num_bytes_to_read,
            int64_t offset,
            Whence whence,
            const ReadCallback& callback) override;
  void Write(Array<uint8_t> bytes_to_write,
             int64_t offset,
             Whence whence,
             const WriteCallback& callback) override;
  void ReadToStream(ScopedDataPipeProducerHandle source,
                    int64_t offset,
                    Whence whence,
                    int64_t num_bytes_to_read,
                    const ReadToStreamCallback& callback) override;
  void WriteFromStream(ScopedDataPipeConsumerHandle sink,
                       int64_t offset,
                       Whence whence,
                       const WriteFromStreamCallback& callback) override;
  void Tell(const TellCallback& callback) override;
  void Seek(int64_t offset,
            Whence whence,
            const SeekCallback& callback) override;
  void Stat(const StatCallback& callback) override;
  void Truncate(int64_t size, const TruncateCallback& callback) override;
  void Touch(TimespecOrNowPtr atime,
             TimespecOrNowPtr mtime,
             const TouchCallback& callback) override;
  void Dup(InterfaceRequest<File> file, const DupCallback& callback) override;
  void Reopen(InterfaceRequest<File> file,
              uint32_t open_flags,
              const ReopenCallback& callback) override;
  void AsBuffer(const AsBufferCallback& callback) override;
  void Ioctl(uint32_t request,
             Array<uint32_t> in_values,
             const IoctlCallback& callback) override;

 private:
  StrongBinding<File> binding_;
  base::ScopedFD file_fd_;

  DISALLOW_COPY_AND_ASSIGN(FileImpl);
};

}  // namespace files
}  // namespace mojo

#endif  // SERVICES_FILES_FILE_IMPL_H_
