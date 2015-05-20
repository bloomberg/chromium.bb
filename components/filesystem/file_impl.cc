// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/file_impl.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <limits>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "components/filesystem/shared_impl.h"
#include "components/filesystem/util.h"

static_assert(sizeof(off_t) <= sizeof(int64_t), "off_t too big");
static_assert(sizeof(size_t) >= sizeof(uint32_t), "size_t too small");

namespace mojo {
namespace files {

const size_t kMaxReadSize = 1 * 1024 * 1024;  // 1 MB.

FileImpl::FileImpl(InterfaceRequest<File> request, base::ScopedFD file_fd)
    : binding_(this, request.Pass()), file_fd_(file_fd.Pass()) {
  DCHECK(file_fd_.is_valid());
}

FileImpl::~FileImpl() {
}

void FileImpl::Close(const CloseCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED);
    return;
  }
  int fd_to_try_to_close = file_fd_.release();
  // POSIX.1 (2013) leaves the validity of the FD undefined on EINTR and EIO. On
  // Linux, the FD is always invalidated, so we'll pretend that the close
  // succeeded. (On other Unixes, the situation may be different and possibly
  // totally broken; see crbug.com/269623.)
  if (IGNORE_EINTR(close(fd_to_try_to_close)) != 0) {
    // Save errno, since we do a few things and we don't want it trampled.
    int error = errno;
    CHECK_NE(error, EBADF);   // This should never happen.
    DCHECK_NE(error, EINTR);  // We already ignored EINTR.
    // I don't know what Linux does on EIO (or any other errors) -- POSIX leaves
    // it undefined -- so report the error and hope that the FD was invalidated.
    callback.Run(ErrnoToError(error));
    return;
  }

  callback.Run(ERROR_OK);
}

// TODO(vtl): Move the implementation to a thread pool.
void FileImpl::Read(uint32_t num_bytes_to_read,
                    int64_t offset,
                    Whence whence,
                    const ReadCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED, Array<uint8_t>());
    return;
  }
  if (num_bytes_to_read > kMaxReadSize) {
    callback.Run(ERROR_OUT_OF_RANGE, Array<uint8_t>());
    return;
  }
  if (Error error = IsOffsetValid(offset)) {
    callback.Run(error, Array<uint8_t>());
    return;
  }
  if (Error error = IsWhenceValid(whence)) {
    callback.Run(error, Array<uint8_t>());
    return;
  }

  if (offset != 0 || whence != WHENCE_FROM_CURRENT) {
    // TODO(vtl): Use |pread()| below in the |WHENCE_FROM_START| case. This
    // implementation is obviously not atomic. (If someone seeks simultaneously,
    // we'll end up writing somewhere else. Or, well, we would if we were
    // multithreaded.) Maybe we should do an |ftell()| and always use |pread()|.
    // TODO(vtl): Possibly, at least sometimes we should not change the file
    // position. See TODO in file.mojom.
    if (lseek(file_fd_.get(), static_cast<off_t>(offset),
              WhenceToStandardWhence(whence)) < 0) {
      callback.Run(ErrnoToError(errno), Array<uint8_t>());
      return;
    }
  }

  Array<uint8_t> bytes_read(num_bytes_to_read);
  ssize_t num_bytes_read = HANDLE_EINTR(
      read(file_fd_.get(), &bytes_read.front(), num_bytes_to_read));
  if (num_bytes_read < 0) {
    callback.Run(ErrnoToError(errno), Array<uint8_t>());
    return;
  }

  DCHECK_LE(static_cast<size_t>(num_bytes_read), num_bytes_to_read);
  bytes_read.resize(static_cast<size_t>(num_bytes_read));
  callback.Run(ERROR_OK, bytes_read.Pass());
}

// TODO(vtl): Move the implementation to a thread pool.
void FileImpl::Write(Array<uint8_t> bytes_to_write,
                     int64_t offset,
                     Whence whence,
                     const WriteCallback& callback) {
  DCHECK(!bytes_to_write.is_null());

  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED, 0);
    return;
  }
  // Who knows what |write()| would return if the size is that big (and it
  // actually wrote that much).
  if (bytes_to_write.size() >
      static_cast<size_t>(std::numeric_limits<ssize_t>::max())) {
    callback.Run(ERROR_OUT_OF_RANGE, 0);
    return;
  }
  if (Error error = IsOffsetValid(offset)) {
    callback.Run(error, 0);
    return;
  }
  if (Error error = IsWhenceValid(whence)) {
    callback.Run(error, 0);
    return;
  }

  if (offset != 0 || whence != WHENCE_FROM_CURRENT) {
    // TODO(vtl): Use |pwrite()| below in the |WHENCE_FROM_START| case. This
    // implementation is obviously not atomic. (If someone seeks simultaneously,
    // we'll end up writing somewhere else. Or, well, we would if we were
    // multithreaded.) Maybe we should do an |ftell()| and always use
    // |pwrite()|.
    // TODO(vtl): Possibly, at least sometimes we should not change the file
    // position. See TODO in file.mojom.
    if (lseek(file_fd_.get(), static_cast<off_t>(offset),
              WhenceToStandardWhence(whence)) < 0) {
      callback.Run(ErrnoToError(errno), 0);
      return;
    }
  }

  const void* buf =
      (bytes_to_write.size() > 0) ? &bytes_to_write.front() : nullptr;
  ssize_t num_bytes_written =
      HANDLE_EINTR(write(file_fd_.get(), buf, bytes_to_write.size()));
  if (num_bytes_written < 0) {
    callback.Run(ErrnoToError(errno), 0);
    return;
  }

  DCHECK_LE(static_cast<size_t>(num_bytes_written),
            std::numeric_limits<uint32_t>::max());
  callback.Run(ERROR_OK, static_cast<uint32_t>(num_bytes_written));
}

void FileImpl::ReadToStream(ScopedDataPipeProducerHandle source,
                            int64_t offset,
                            Whence whence,
                            int64_t num_bytes_to_read,
                            const ReadToStreamCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED);
    return;
  }
  if (Error error = IsOffsetValid(offset)) {
    callback.Run(error);
    return;
  }
  if (Error error = IsWhenceValid(whence)) {
    callback.Run(error);
    return;
  }

  // TODO(vtl): FIXME soon
  NOTIMPLEMENTED();
  callback.Run(ERROR_UNIMPLEMENTED);
}

void FileImpl::WriteFromStream(ScopedDataPipeConsumerHandle sink,
                               int64_t offset,
                               Whence whence,
                               const WriteFromStreamCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED);
    return;
  }
  if (Error error = IsOffsetValid(offset)) {
    callback.Run(error);
    return;
  }
  if (Error error = IsWhenceValid(whence)) {
    callback.Run(error);
    return;
  }

  // TODO(vtl): FIXME soon
  NOTIMPLEMENTED();
  callback.Run(ERROR_UNIMPLEMENTED);
}

void FileImpl::Tell(const TellCallback& callback) {
  Seek(0, WHENCE_FROM_CURRENT, callback);
}

void FileImpl::Seek(int64_t offset,
                    Whence whence,
                    const SeekCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED, 0);
    return;
  }
  if (Error error = IsOffsetValid(offset)) {
    callback.Run(error, 0);
    return;
  }
  if (Error error = IsWhenceValid(whence)) {
    callback.Run(error, 0);
    return;
  }

  off_t position = lseek(file_fd_.get(), static_cast<off_t>(offset),
                         WhenceToStandardWhence(whence));
  if (position < 0) {
    callback.Run(ErrnoToError(errno), 0);
    return;
  }

  callback.Run(ERROR_OK, static_cast<int64>(position));
}

void FileImpl::Stat(const StatCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED, nullptr);
    return;
  }
  StatFD(file_fd_.get(), FILE_TYPE_REGULAR_FILE, callback);
}

void FileImpl::Truncate(int64_t size, const TruncateCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED);
    return;
  }
  if (size < 0) {
    callback.Run(ERROR_INVALID_ARGUMENT);
    return;
  }
  if (Error error = IsOffsetValid(size)) {
    callback.Run(error);
    return;
  }

  if (ftruncate(file_fd_.get(), static_cast<off_t>(size)) != 0) {
    callback.Run(ErrnoToError(errno));
    return;
  }

  callback.Run(ERROR_OK);
}

void FileImpl::Touch(TimespecOrNowPtr atime,
                     TimespecOrNowPtr mtime,
                     const TouchCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED);
    return;
  }
  TouchFD(file_fd_.get(), atime.Pass(), mtime.Pass(), callback);
}

void FileImpl::Dup(InterfaceRequest<File> file, const DupCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED);
    return;
  }

  base::ScopedFD file_fd(dup(file_fd_.get()));
  if (!file_fd.is_valid()) {
    callback.Run(ErrnoToError(errno));
    return;
  }

  new FileImpl(file.Pass(), file_fd.Pass());
  callback.Run(ERROR_OK);
}

void FileImpl::Reopen(InterfaceRequest<File> file,
                      uint32_t open_flags,
                      const ReopenCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED);
    return;
  }

  // TODO(vtl): FIXME soon
  NOTIMPLEMENTED();
  callback.Run(ERROR_UNIMPLEMENTED);
}

void FileImpl::AsBuffer(const AsBufferCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED, ScopedSharedBufferHandle());
    return;
  }

  // TODO(vtl): FIXME soon
  NOTIMPLEMENTED();
  callback.Run(ERROR_UNIMPLEMENTED, ScopedSharedBufferHandle());
}

void FileImpl::Ioctl(uint32_t request,
                     Array<uint32_t> in_values,
                     const IoctlCallback& callback) {
  if (!file_fd_.is_valid()) {
    callback.Run(ERROR_CLOSED, Array<uint32_t>());
    return;
  }

  // TODO(vtl): The "correct" error code should be one that can be translated to
  // ENOTTY!
  callback.Run(ERROR_UNAVAILABLE, Array<uint32_t>());
}

}  // namespace files
}  // namespace mojo
