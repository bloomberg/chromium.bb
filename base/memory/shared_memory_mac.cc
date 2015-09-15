// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/safe_strerror.h"
#include "base/process/process_metrics.h"
#include "base/profiler/scoped_tracker.h"
#include "base/scoped_generic.h"
#include "base/strings/utf_string_conversions.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif  // OS_MACOSX

namespace base {

namespace {

struct ScopedPathUnlinkerTraits {
  static FilePath* InvalidValue() { return nullptr; }

  static void Free(FilePath* path) {
    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466437
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "466437 SharedMemory::Create::Unlink"));
    if (unlink(path->value().c_str()))
      PLOG(WARNING) << "unlink";
  }
};

// Unlinks the FilePath when the object is destroyed.
typedef ScopedGeneric<FilePath*, ScopedPathUnlinkerTraits> ScopedPathUnlinker;

// Makes a temporary file, fdopens it, and then unlinks it. |fp| is populated
// with the fdopened FILE. |readonly_fd| is populated with the opened fd if
// options.share_read_only is true. |path| is populated with the location of
// the file before it was unlinked.
// Returns false if there's an unhandled failure.
bool CreateAnonymousSharedMemory(const SharedMemoryCreateOptions& options,
                                 ScopedFILE* fp,
                                 ScopedFD* readonly_fd,
                                 FilePath* path) {
  // Q: Why not use the shm_open() etc. APIs?
  // A: Because they're limited to 4mb on OS X.  FFFFFFFUUUUUUUUUUU
  FilePath directory;
  ScopedPathUnlinker path_unlinker;
  if (GetShmemTempDir(options.executable, &directory)) {
    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466437
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "466437 SharedMemory::Create::OpenTemporaryFile"));
    fp->reset(CreateAndOpenTemporaryFileInDir(directory, path));

    // Deleting the file prevents anyone else from mapping it in (making it
    // private), and prevents the need for cleanup (once the last fd is
    // closed, it is truly freed).
    if (*fp)
      path_unlinker.reset(path);
  }

  if (*fp) {
    if (options.share_read_only) {
      // TODO(erikchen): Remove ScopedTracker below once
      // http://crbug.com/466437 is fixed.
      tracked_objects::ScopedTracker tracking_profile(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "466437 SharedMemory::Create::OpenReadonly"));
      // Also open as readonly so that we can ShareReadOnlyToProcess.
      readonly_fd->reset(HANDLE_EINTR(open(path->value().c_str(), O_RDONLY)));
      if (!readonly_fd->is_valid()) {
        DPLOG(ERROR) << "open(\"" << path->value() << "\", O_RDONLY) failed";
        fp->reset();
        return false;
      }
    }
  }
  return true;
}

}  // namespace

SharedMemory::SharedMemory()
    : mapped_file_(-1),
      readonly_mapped_file_(-1),
      mapped_size_(0),
      memory_(NULL),
      read_only_(false),
      requested_size_(0) {
}

SharedMemory::SharedMemory(const SharedMemoryHandle& handle, bool read_only)
    : mapped_file_(GetFdFromSharedMemoryHandle(handle)),
      readonly_mapped_file_(-1),
      mapped_size_(0),
      memory_(NULL),
      read_only_(read_only),
      requested_size_(0) {
}

SharedMemory::SharedMemory(const SharedMemoryHandle& handle,
                           bool read_only,
                           ProcessHandle process)
    : mapped_file_(GetFdFromSharedMemoryHandle(handle)),
      readonly_mapped_file_(-1),
      mapped_size_(0),
      memory_(NULL),
      read_only_(read_only),
      requested_size_(0) {
  // We don't handle this case yet (note the ignored parameter); let's die if
  // someone comes calling.
  NOTREACHED();
}

SharedMemory::~SharedMemory() {
  Unmap();
  Close();
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.IsValid();
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() {
  return SharedMemoryHandle();
}

// static
void SharedMemory::CloseHandle(const SharedMemoryHandle& handle) {
  DCHECK_GE(GetFdFromSharedMemoryHandle(handle), 0);
  if (close(GetFdFromSharedMemoryHandle(handle)) < 0)
    DPLOG(ERROR) << "close";
}

// static
size_t SharedMemory::GetHandleLimit() {
  return GetMaxFds();
}

// static
SharedMemoryHandle SharedMemory::DuplicateHandle(
    const SharedMemoryHandle& handle) {
  return handle.Duplicate();
}

// static
int SharedMemory::GetFdFromSharedMemoryHandle(
    const SharedMemoryHandle& handle) {
  return handle.GetFileDescriptor().fd;
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

// static
int SharedMemory::GetSizeFromSharedMemoryHandle(
    const SharedMemoryHandle& handle) {
  struct stat st;
  if (fstat(GetFdFromSharedMemoryHandle(handle), &st) != 0)
    return -1;
  return st.st_size;
}

// Chromium mostly only uses the unique/private shmem as specified by
// "name == L"". The exception is in the StatsTable.
// TODO(jrg): there is no way to "clean up" all unused named shmem if
// we restart from a crash.  (That isn't a new problem, but it is a problem.)
// In case we want to delete it later, it may be useful to save the value
// of mem_filename after FilePathForMemoryName().
bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466437
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466437 SharedMemory::Create::Start"));
  DCHECK_EQ(-1, mapped_file_);
  if (options.size == 0) return false;

  if (options.size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  // This function theoretically can block on the disk, but realistically
  // the temporary files we create will just go into the buffer cache
  // and be deleted before they ever make it out to disk.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  ScopedFILE fp;
  ScopedFD readonly_fd;

  FilePath path;
  bool result = CreateAnonymousSharedMemory(options, &fp, &readonly_fd, &path);
  if (!result)
    return false;

  if (!fp) {
    PLOG(ERROR) << "Creating shared memory in " << path.value() << " failed";
    return false;
  }

  // Get current size.
  struct stat stat;
  if (fstat(fileno(fp.get()), &stat) != 0)
    return false;
  const size_t current_size = stat.st_size;
  if (current_size != options.size) {
    if (HANDLE_EINTR(ftruncate(fileno(fp.get()), options.size)) != 0)
      return false;
  }
  requested_size_ = options.size;

  return PrepareMapFile(fp.Pass(), readonly_fd.Pass());
}

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (mapped_file_ == -1)
    return false;

  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  if (memory_)
    return false;

  memory_ = mmap(NULL, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
                 MAP_SHARED, mapped_file_, offset);

  bool mmap_succeeded = memory_ && memory_ != reinterpret_cast<void*>(-1);
  if (mmap_succeeded) {
    mapped_size_ = bytes;
    DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(memory_) &
        (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
  } else {
    memory_ = NULL;
  }

  return mmap_succeeded;
}

bool SharedMemory::Unmap() {
  if (memory_ == NULL)
    return false;

  munmap(memory_, mapped_size_);
  memory_ = NULL;
  mapped_size_ = 0;
  return true;
}

SharedMemoryHandle SharedMemory::handle() const {
  return SharedMemoryHandle(mapped_file_, false);
}

void SharedMemory::Close() {
  if (mapped_file_ > 0) {
    if (close(mapped_file_) < 0)
      PLOG(ERROR) << "close";
    mapped_file_ = -1;
  }
  if (readonly_mapped_file_ > 0) {
    if (close(readonly_mapped_file_) < 0)
      PLOG(ERROR) << "close";
    readonly_mapped_file_ = -1;
  }
}

bool SharedMemory::PrepareMapFile(ScopedFILE fp, ScopedFD readonly_fd) {
  DCHECK_EQ(-1, mapped_file_);
  DCHECK_EQ(-1, readonly_mapped_file_);
  if (fp == NULL)
    return false;

  // This function theoretically can block on the disk, but realistically
  // the temporary files we create will just go into the buffer cache
  // and be deleted before they ever make it out to disk.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  struct stat st = {};
  if (fstat(fileno(fp.get()), &st))
    NOTREACHED();
  if (readonly_fd.is_valid()) {
    struct stat readonly_st = {};
    if (fstat(readonly_fd.get(), &readonly_st))
      NOTREACHED();
    if (st.st_dev != readonly_st.st_dev || st.st_ino != readonly_st.st_ino) {
      LOG(ERROR) << "writable and read-only inodes don't match; bailing";
      return false;
    }
  }

  mapped_file_ = HANDLE_EINTR(dup(fileno(fp.get())));
  if (mapped_file_ == -1) {
    if (errno == EMFILE) {
      LOG(WARNING) << "Shared memory creation failed; out of file descriptors";
      return false;
    } else {
      NOTREACHED() << "Call to dup failed, errno=" << errno;
    }
  }
  readonly_mapped_file_ = readonly_fd.release();

  return true;
}

// For the given shmem named |mem_name|, return a filename to mmap()
// (and possibly create).  Modifies |filename|.  Return false on
// error, or true of we are happy.
bool SharedMemory::FilePathForMemoryName(const std::string& mem_name,
                                         FilePath* path) {
  // mem_name will be used for a filename; make sure it doesn't
  // contain anything which will confuse us.
  DCHECK_EQ(std::string::npos, mem_name.find('/'));
  DCHECK_EQ(std::string::npos, mem_name.find('\0'));

  FilePath temp_dir;
  if (!GetShmemTempDir(false, &temp_dir))
    return false;

  std::string name_base = std::string(mac::BaseBundleID());
  *path = temp_dir.AppendASCII(name_base + ".shmem." + mem_name);
  return true;
}

bool SharedMemory::ShareToProcessCommon(ProcessHandle process,
                                        SharedMemoryHandle* new_handle,
                                        bool close_self,
                                        ShareMode share_mode) {
  int handle_to_dup = -1;
  switch (share_mode) {
    case SHARE_CURRENT_MODE:
      handle_to_dup = mapped_file_;
      break;
    case SHARE_READONLY:
      // We could imagine re-opening the file from /dev/fd, but that can't make
      // it readonly on Mac: https://codereview.chromium.org/27265002/#msg10
      CHECK_GE(readonly_mapped_file_, 0);
      handle_to_dup = readonly_mapped_file_;
      break;
  }

  const int new_fd = HANDLE_EINTR(dup(handle_to_dup));
  if (new_fd < 0) {
    DPLOG(ERROR) << "dup() failed.";
    return false;
  }

  new_handle->SetFileHandle(new_fd, true);

  if (close_self) {
    Unmap();
    Close();
  }

  return true;
}

}  // namespace base
