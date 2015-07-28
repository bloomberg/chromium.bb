// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/file_impl.h"

#include <stdint.h>
#include <limits>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "components/filesystem/util.h"
#include "mojo/platform_handle/platform_handle_functions.h"

static_assert(sizeof(off_t) <= sizeof(int64_t), "off_t too big");
static_assert(sizeof(size_t) >= sizeof(uint32_t), "size_t too small");

using base::Time;
using mojo::ScopedHandle;

namespace filesystem {

const size_t kMaxReadSize = 1 * 1024 * 1024;  // 1 MB.

FileImpl::FileImpl(mojo::InterfaceRequest<File> request,
                   const base::FilePath& path,
                   uint32 flags)
    : binding_(this, request.Pass()), file_(path, flags) {
  DCHECK(file_.IsValid());
}

FileImpl::FileImpl(mojo::InterfaceRequest<File> request, base::File file)
    : binding_(this, request.Pass()), file_(file.Pass()) {
  DCHECK(file_.IsValid());
}

FileImpl::~FileImpl() {
}

void FileImpl::Close(const CloseCallback& callback) {
  if (!file_.IsValid()) {
    callback.Run(GetError(file_));
    return;
  }

  file_.Close();
  callback.Run(FILE_ERROR_OK);
}

// TODO(vtl): Move the implementation to a thread pool.
void FileImpl::Read(uint32_t num_bytes_to_read,
                    int64_t offset,
                    Whence whence,
                    const ReadCallback& callback) {
  if (!file_.IsValid()) {
    callback.Run(GetError(file_), mojo::Array<uint8_t>());
    return;
  }
  if (num_bytes_to_read > kMaxReadSize) {
    callback.Run(FILE_ERROR_INVALID_OPERATION, mojo::Array<uint8_t>());
    return;
  }
  if (FileError error = IsOffsetValid(offset)) {
    callback.Run(error, mojo::Array<uint8_t>());
    return;
  }
  if (FileError error = IsWhenceValid(whence)) {
    callback.Run(error, mojo::Array<uint8_t>());
    return;
  }

  if (file_.Seek(static_cast<base::File::Whence>(whence), offset) == -1) {
    callback.Run(FILE_ERROR_FAILED, mojo::Array<uint8_t>());
    return;
  }

  mojo::Array<uint8_t> bytes_read(num_bytes_to_read);
  int num_bytes_read = file_.ReadAtCurrentPos(
      reinterpret_cast<char*>(&bytes_read.front()), num_bytes_to_read);
  if (num_bytes_read < 0) {
    callback.Run(FILE_ERROR_FAILED, mojo::Array<uint8_t>());
    return;
  }

  DCHECK_LE(static_cast<size_t>(num_bytes_read), num_bytes_to_read);
  bytes_read.resize(static_cast<size_t>(num_bytes_read));
  callback.Run(FILE_ERROR_OK, bytes_read.Pass());
}

// TODO(vtl): Move the implementation to a thread pool.
void FileImpl::Write(mojo::Array<uint8_t> bytes_to_write,
                     int64_t offset,
                     Whence whence,
                     const WriteCallback& callback) {
  DCHECK(!bytes_to_write.is_null());
  if (!file_.IsValid()) {
    callback.Run(GetError(file_), 0);
    return;
  }
  // Who knows what |write()| would return if the size is that big (and it
  // actually wrote that much).
  if (bytes_to_write.size() >
#if defined(OS_WIN)
      static_cast<size_t>(std::numeric_limits<int>::max())) {
#else
      static_cast<size_t>(std::numeric_limits<ssize_t>::max())) {
#endif
    callback.Run(FILE_ERROR_INVALID_OPERATION, 0);
    return;
  }
  if (FileError error = IsOffsetValid(offset)) {
    callback.Run(error, 0);
    return;
  }
  if (FileError error = IsWhenceValid(whence)) {
    callback.Run(error, 0);
    return;
  }

  if (file_.Seek(static_cast<base::File::Whence>(whence), offset) == -1) {
    callback.Run(FILE_ERROR_FAILED, 0);
    return;
  }

  const char* buf = (bytes_to_write.size() > 0)
                        ? reinterpret_cast<char*>(&bytes_to_write.front())
                        : nullptr;
  int num_bytes_written = file_.WriteAtCurrentPos(
      buf, static_cast<int>(bytes_to_write.size()));
  if (num_bytes_written < 0) {
    callback.Run(FILE_ERROR_FAILED, 0);
    return;
  }

  DCHECK_LE(static_cast<size_t>(num_bytes_written),
            std::numeric_limits<uint32_t>::max());
  callback.Run(FILE_ERROR_OK, static_cast<uint32_t>(num_bytes_written));
}

void FileImpl::Tell(const TellCallback& callback) {
  Seek(0, WHENCE_FROM_CURRENT, callback);
}

void FileImpl::Seek(int64_t offset,
                    Whence whence,
                    const SeekCallback& callback) {
  if (!file_.IsValid()) {
    callback.Run(GetError(file_), 0);
    return;
  }
  if (FileError error = IsOffsetValid(offset)) {
    callback.Run(error, 0);
    return;
  }
  if (FileError error = IsWhenceValid(whence)) {
    callback.Run(error, 0);
    return;
  }

  int64 position = file_.Seek(static_cast<base::File::Whence>(whence), offset);
  if (position < 0) {
    callback.Run(FILE_ERROR_FAILED, 0);
    return;
  }

  callback.Run(FILE_ERROR_OK, static_cast<int64>(position));
}

void FileImpl::Stat(const StatCallback& callback) {
  if (!file_.IsValid()) {
    callback.Run(GetError(file_), nullptr);
    return;
  }

  base::File::Info info;
  if (!file_.GetInfo(&info)) {
    callback.Run(FILE_ERROR_FAILED, nullptr);
    return;
  }

  callback.Run(FILE_ERROR_OK, MakeFileInformation(info).Pass());
}

void FileImpl::Truncate(int64_t size, const TruncateCallback& callback) {
  if (!file_.IsValid()) {
    callback.Run(GetError(file_));
    return;
  }
  if (size < 0) {
    callback.Run(FILE_ERROR_INVALID_OPERATION);
    return;
  }
  if (FileError error = IsOffsetValid(size)) {
    callback.Run(error);
    return;
  }

  if (!file_.SetLength(size)) {
    callback.Run(FILE_ERROR_NOT_FOUND);
    return;
  }

  callback.Run(FILE_ERROR_OK);
}

void FileImpl::Touch(TimespecOrNowPtr atime,
                     TimespecOrNowPtr mtime,
                     const TouchCallback& callback) {
  if (!file_.IsValid()) {
    callback.Run(GetError(file_));
    return;
  }

  base::Time base_atime = Time::Now();
  if (!atime) {
    base::File::Info info;
    if (!file_.GetInfo(&info)) {
      callback.Run(FILE_ERROR_FAILED);
      return;
    }

    base_atime = info.last_accessed;
  } else if (!atime->now) {
    base_atime = Time::FromDoubleT(atime->seconds);
  }

  base::Time base_mtime = Time::Now();
  if (!mtime) {
    base::File::Info info;
    if (!file_.GetInfo(&info)) {
      callback.Run(FILE_ERROR_FAILED);
      return;
    }

    base_mtime = info.last_modified;
  } else if (!mtime->now) {
    base_mtime = Time::FromDoubleT(mtime->seconds);
  }

  file_.SetTimes(base_atime, base_mtime);
  callback.Run(FILE_ERROR_OK);
}

void FileImpl::Dup(mojo::InterfaceRequest<File> file,
                   const DupCallback& callback) {
  if (!file_.IsValid()) {
    callback.Run(GetError(file_));
    return;
  }

  base::File new_file = file_.Duplicate();
  if (!new_file.IsValid()) {
    callback.Run(GetError(new_file));
    return;
  }

  if (file.is_pending())
    new FileImpl(file.Pass(), new_file.Pass());
  callback.Run(FILE_ERROR_OK);
}

void FileImpl::Flush(const FlushCallback& callback) {
  if (!file_.IsValid()) {
    callback.Run(GetError(file_));
    return;
  }

  bool ret = file_.Flush();
  callback.Run(ret ? FILE_ERROR_OK : FILE_ERROR_FAILED);
}

void FileImpl::AsHandle(const AsHandleCallback& callback) {
  if (!file_.IsValid()) {
    callback.Run(GetError(file_), ScopedHandle());
    return;
  }

  base::File new_file = file_.Duplicate();
  if (!new_file.IsValid()) {
    callback.Run(GetError(new_file), ScopedHandle());
    return;
  }

  base::File::Info info;
  if (!new_file.GetInfo(&info)) {
    callback.Run(FILE_ERROR_FAILED, ScopedHandle());
    return;
  }

  // Perform one additional check right before we send the file's file
  // descriptor over mojo. This is theoretically redundant, but given that
  // passing a file descriptor to a directory is a sandbox escape on Windows,
  // we should be absolutely paranoid.
  if (info.is_directory) {
    callback.Run(FILE_ERROR_NOT_A_FILE, ScopedHandle());
    return;
  }

  MojoHandle mojo_handle;
  MojoResult create_result = MojoCreatePlatformHandleWrapper(
      new_file.TakePlatformFile(), &mojo_handle);
  if (create_result != MOJO_RESULT_OK) {
    callback.Run(FILE_ERROR_FAILED, ScopedHandle());
    return;
  }

  callback.Run(FILE_ERROR_OK, ScopedHandle(mojo::Handle(mojo_handle)).Pass());
}

}  // namespace filesystem
