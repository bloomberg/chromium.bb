// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mapped_file.h"

#include <stdlib.h>
#include <sys/mman.h>

#include <map>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_local.h"

// This implementation of MappedFile doesn't use a shared RW mmap for
// performance reason. Instead it will use a private RO mmap and install a SEGV
// signal handler. When the memory is modified, the handler will register the
// modified address and change the protection of the page to RW. When a flush is
// then executed, it has access to the exact pages that have been modified and
// will only write those to disk. The handler will only track half of the dirty
// pages. If more than half the pages are modified, the flush will instead write
// the full buffer to disk.

namespace {

// Choose 4k as a reasonable page size. As this file is used mainly on Android,
// this is the real android page size.
const size_t kPageSize = 4096;

// Variable capacity array, optimized for capacity of 1. Most of the mapped file
// are used to map exactly 2 pages. Tracking 1 page is then optimal because if
// both pages are modified, writing the full view is the optimal behavior.
class SmallArray {
 public:
  SmallArray() : capacity_(0), array_(NULL) {}
  ~SmallArray() { SetCapacity(0); }

  size_t capacity() { return capacity_; }
  char** array() { return array_; }
  void SetCapacity(size_t capacity) {
    if (capacity_ > 1)
      delete[] array_;
    capacity_ = capacity;
    if (capacity > 1)
      array_ = new char*[capacity];
    else
      array_ = small_array_;
  }

 private:
  size_t capacity_;
  char** array_;
  char* small_array_[1];
};

// Information about the memory mapped part of a file.
struct MappedFileInfo {
  // Stat address of the memory.
  char* start_address;
  // Size of the memory map.
  size_t size;
  // Number of dirty page. A page is dirty if the memory content is different
  // from the file content.
  size_t num_dirty_pages;
  // The  dirty pages.
  SmallArray dirty_pages;
};

// The maximum number of dirty pages that can be tracked. Limit the memory
// overhead to 2kb per file.
const size_t kMaxDirtyPagesCacheSize =
    kPageSize / sizeof(char*) / 2 - sizeof(MappedFileInfo);

class ThreadLocalMappedFileInfo {
 public:
  ThreadLocalMappedFileInfo() {}
  ~ThreadLocalMappedFileInfo() {}

  void RegisterMappedFile(disk_cache::MappedFile* mapped_file, size_t size) {
    scoped_ptr<MappedFileInfo> new_info(new MappedFileInfo);
    new_info->start_address = static_cast<char*>(mapped_file->buffer());
    new_info->size = size;
    new_info->num_dirty_pages = 0;
    // Track half of the dirty pages, after this, just overwrite the full
    // content.
    size_t capacity = (size + kPageSize - 1) / kPageSize / 2;
    if (capacity > kMaxDirtyPagesCacheSize)
      capacity = kMaxDirtyPagesCacheSize;
    new_info->dirty_pages.SetCapacity(capacity);
    info_per_map_file_[mapped_file] = new_info.get();
    infos_.push_back(new_info.release());
    Update();
  }

  void UnregisterMappedFile(disk_cache::MappedFile* mapped_file) {
    MappedFileInfo* info = InfoForMappedFile(mapped_file);
    DCHECK(info);
    info_per_map_file_.erase(mapped_file);
    infos_.erase(std::find(infos_.begin(), infos_.end(), info));
    Update();
  }

  MappedFileInfo* InfoForMappedFile(disk_cache::MappedFile* mapped_file) {
    return info_per_map_file_[mapped_file];
  }

  MappedFileInfo** infos_ptr() { return infos_ptr_; }
  size_t infos_size() { return infos_size_; }

 private:
  // Update |infos_ptr_| and |infos_size_| when |infos_| change.
  void Update() {
    infos_ptr_ = &infos_[0];
    infos_size_ = infos_.size();
  }

  // Link to the MappedFileInfo for a given MappedFile.
  std::map<disk_cache::MappedFile*, MappedFileInfo*> info_per_map_file_;
  // Vector of information about all current MappedFile belonging to the current
  // thread.
  ScopedVector<MappedFileInfo> infos_;
  // Pointer to the storage part of |infos_|. This is kept as a variable to
  // prevent the signal handler from calling any C++ method that might allocate
  // memory.
  MappedFileInfo** infos_ptr_;
  // Size of |infos_|.
  size_t infos_size_;
};

class SegvHandler {
 public:
  // Register the signal handler.
  SegvHandler();
  ~SegvHandler() {}

  // SEGV signal handler. This handler will check that the address that
  // generated the fault is one associated with a mapped file. If that's the
  // case, it will register the address and change the protection to RW then
  // return. This will cause the instruction that generated the fault to be
  // re-executed. If not, it will just reinstall the old handler and return,
  // which will generate the fault again and let the initial handler get called.
  static void SigSegvHandler(int sig, siginfo_t* si, void* unused);

  base::ThreadLocalPointer<ThreadLocalMappedFileInfo>& thread_local_infos() {
    return thread_local_infos_;
  }

 private:
  // Install the SEGV handler, storing the current sigaction in |old_sigaction|
  // if it is not NULL.
  static void InstallSigHandler(struct sigaction* old_sigaction);

  base::ThreadLocalPointer<ThreadLocalMappedFileInfo> thread_local_infos_;
  struct sigaction old_sigaction_;
};

static base::LazyInstance<SegvHandler> g_segv_handler =
    LAZY_INSTANCE_INITIALIZER;

// Initialisation method.
SegvHandler::SegvHandler() {
  // Setup the SIGV signal handler.
  InstallSigHandler(&old_sigaction_);
}

// static
void SegvHandler::SigSegvHandler(int sig, siginfo_t* si, void* unused) {
  // First, check if the current sighandler has the SA_SIGINFO flag. If it
  // doesn't it means an external library installed temporarly a signal handler
  // using signal, and so incorrectly restored the current one. The parameters
  // are then useless.
  struct sigaction current_action;
  sigaction(SIGSEGV, NULL, &current_action);
  if (!(current_action.sa_flags & SA_SIGINFO)) {
    LOG(WARNING) << "Signal handler have been re-installed incorrectly.";
    InstallSigHandler(NULL);
    // Returning will re-run the signal with the correct parameters.
    return;
  }
  ThreadLocalMappedFileInfo* thread_local_info =
      g_segv_handler.Pointer()->thread_local_infos().Get();
  if (thread_local_info) {
    char* addr = reinterpret_cast<char*>(si->si_addr);
    for (size_t i = 0; i < thread_local_info->infos_size(); ++i) {
      MappedFileInfo* info = thread_local_info->infos_ptr()[i];
      if (info->start_address <= addr &&
          addr < info->start_address + info->size) {
        // Only track new dirty pages if the array has still some capacity.
        // Otherwise, the full buffer will be written to disk and it is not
        // necessary to track changes until the next flush.
        if (info->num_dirty_pages < info->dirty_pages.capacity()) {
          char* aligned_address = reinterpret_cast<char*>(
              reinterpret_cast<size_t>(addr) & ~(kPageSize - 1));
          mprotect(aligned_address, kPageSize, PROT_READ | PROT_WRITE);
          info->dirty_pages.array()[info->num_dirty_pages] = aligned_address;
        } else {
          mprotect(info->start_address, info->size, PROT_READ | PROT_WRITE);
        }
        info->num_dirty_pages++;
        return;
      }
    }
  }
  // The address it not handled by any mapped filed. Let the default handler get
  // called.
  sigaction(SIGSEGV, &g_segv_handler.Pointer()->old_sigaction_, NULL);
}

// static
void SegvHandler::InstallSigHandler(struct sigaction* old_sigaction) {
  struct sigaction action;
  action.sa_sigaction = SigSegvHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGSEGV, &action, old_sigaction);
}

}  // namespace

namespace disk_cache {

void* MappedFile::Init(const base::FilePath& name, size_t size) {
  DCHECK(!init_);
  if (init_ || !File::Init(name))
    return NULL;

  if (!size)
    size = GetLength();

  buffer_ = mmap(NULL, size, PROT_READ, MAP_PRIVATE, platform_file(), 0);
  if (reinterpret_cast<ptrdiff_t>(buffer_) == -1) {
    NOTREACHED();
    buffer_ = 0;
  }

  if (buffer_) {
    ThreadLocalMappedFileInfo* thread_local_info =
        g_segv_handler.Pointer()->thread_local_infos().Get();
    if (!thread_local_info) {
      thread_local_info = new ThreadLocalMappedFileInfo();
      g_segv_handler.Pointer()->thread_local_infos().Set(thread_local_info);
    }
    DCHECK(size);
    thread_local_info->RegisterMappedFile(this, size);
  }

  init_ = true;
  view_size_ = size;
  return buffer_;
}

bool MappedFile::Load(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Read(block->buffer(), block->size(), offset);
}

bool MappedFile::Store(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Write(block->buffer(), block->size(), offset);
}

void MappedFile::Flush() {
  DCHECK(buffer_);
  MappedFileInfo* info = g_segv_handler.Pointer()->thread_local_infos().Get()->
      InfoForMappedFile(this);
  DCHECK(info);
  if (info->num_dirty_pages > info->dirty_pages.capacity()) {
    Write(buffer_, view_size_, 0);
  } else {
    const char* buffer_ptr = static_cast<const char*>(buffer_);
    for (size_t i = 0; i < info->num_dirty_pages; ++i) {
      const char* ptr = info->dirty_pages.array()[i];
      size_t size_to_write = kPageSize;
      // The view_size is not a full number of page. Only write the fraction of
      // the page that is in the view.
      if (ptr - buffer_ptr + kPageSize > view_size_)
        size_to_write = view_size_ - (ptr - buffer_ptr);
      Write(ptr, size_to_write, ptr - buffer_ptr);
    }
  }
  info->num_dirty_pages = 0;
  mprotect(buffer_, view_size_, PROT_READ);
}

MappedFile::~MappedFile() {
  if (!init_)
    return;

  if (buffer_) {
    Flush();
    ThreadLocalMappedFileInfo* thread_local_info =
        g_segv_handler.Pointer()->thread_local_infos().Get();
    DCHECK(thread_local_info);
    thread_local_info->UnregisterMappedFile(this);
    munmap(buffer_, 0);
  }
}

}  // namespace disk_cache
