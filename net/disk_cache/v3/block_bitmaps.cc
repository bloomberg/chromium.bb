// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/block_files.h"

#include "base/atomicops.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/file_lock.h"
#include "net/disk_cache/trace.h"

using base::TimeTicks;

namespace disk_cache {

BlockFiles::BlockFiles(const base::FilePath& path)
    : init_(false), zero_buffer_(NULL), path_(path) {
}

BlockFiles::~BlockFiles() {
  if (zero_buffer_)
    delete[] zero_buffer_;
  CloseFiles();
}

bool BlockFiles::Init(bool create_files) {
  DCHECK(!init_);
  if (init_)
    return false;

  thread_checker_.reset(new base::ThreadChecker);

  block_files_.resize(kFirstAdditionalBlockFile);
  for (int i = 0; i < kFirstAdditionalBlockFile; i++) {
    if (create_files)
      if (!CreateBlockFile(i, static_cast<FileType>(i + 1), true))
        return false;

    if (!OpenBlockFile(i))
      return false;

    // Walk this chain of files removing empty ones.
    if (!RemoveEmptyFile(static_cast<FileType>(i + 1)))
      return false;
  }

  init_ = true;
  return true;
}

bool BlockFiles::CreateBlock(FileType block_type, int block_count,
                             Addr* block_address) {
  DCHECK(thread_checker_->CalledOnValidThread());
  if (block_type < RANKINGS || block_type > BLOCK_4K ||
      block_count < 1 || block_count > 4)
    return false;
  if (!init_)
    return false;

  MappedFile* file = FileForNewBlock(block_type, block_count);
  if (!file)
    return false;

  ScopedFlush flush(file);
  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());

  int target_size = 0;
  for (int i = block_count; i <= 4; i++) {
    if (header->empty[i - 1]) {
      target_size = i;
      break;
    }
  }

  DCHECK(target_size);
  int index;
  if (!CreateMapBlock(target_size, block_count, header, &index))
    return false;

  Addr address(block_type, block_count, header->this_file, index);
  block_address->set_value(address.value());
  Trace("CreateBlock 0x%x", address.value());
  return true;
}

void BlockFiles::DeleteBlock(Addr address, bool deep) {
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!address.is_initialized() || address.is_separate_file())
    return;

  if (!zero_buffer_) {
    zero_buffer_ = new char[Addr::BlockSizeForFileType(BLOCK_4K) * 4];
    memset(zero_buffer_, 0, Addr::BlockSizeForFileType(BLOCK_4K) * 4);
  }
  MappedFile* file = GetFile(address);
  if (!file)
    return;

  Trace("DeleteBlock 0x%x", address.value());

  size_t size = address.BlockSize() * address.num_blocks();
  size_t offset = address.start_block() * address.BlockSize() +
                  kBlockHeaderSize;
  if (deep)
    file->Write(zero_buffer_, size, offset);

  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  DeleteMapBlock(address.start_block(), address.num_blocks(), header);
  file->Flush();

  if (!header->num_entries) {
    // This file is now empty. Let's try to delete it.
    FileType type = Addr::RequiredFileType(header->entry_size);
    if (Addr::BlockSizeForFileType(RANKINGS) == header->entry_size)
      type = RANKINGS;
    RemoveEmptyFile(type);  // Ignore failures.
  }
}

void BlockFiles::CloseFiles() {
  if (init_) {
    DCHECK(thread_checker_->CalledOnValidThread());
  }
  init_ = false;
  for (unsigned int i = 0; i < block_files_.size(); i++) {
    if (block_files_[i]) {
      block_files_[i]->Release();
      block_files_[i] = NULL;
    }
  }
  block_files_.clear();
}

void BlockFiles::ReportStats() {
  DCHECK(thread_checker_->CalledOnValidThread());
  int used_blocks[kFirstAdditionalBlockFile];
  int load[kFirstAdditionalBlockFile];
  for (int i = 0; i < kFirstAdditionalBlockFile; i++) {
    GetFileStats(i, &used_blocks[i], &load[i]);
  }
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_0", used_blocks[0]);
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_1", used_blocks[1]);
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_2", used_blocks[2]);
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_3", used_blocks[3]);

  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_0", load[0], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_1", load[1], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_2", load[2], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_3", load[3], 101);
}

bool BlockFiles::IsValid(Addr address) {
#ifdef NDEBUG
  return true;
#else
  if (!address.is_initialized() || address.is_separate_file())
    return false;

  MappedFile* file = GetFile(address);
  if (!file)
    return false;

  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  bool rv = UsedMapBlock(address.start_block(), address.num_blocks(), header);
  DCHECK(rv);

  static bool read_contents = false;
  if (read_contents) {
    scoped_ptr<char[]> buffer;
    buffer.reset(new char[Addr::BlockSizeForFileType(BLOCK_4K) * 4]);
    size_t size = address.BlockSize() * address.num_blocks();
    size_t offset = address.start_block() * address.BlockSize() +
                    kBlockHeaderSize;
    bool ok = file->Read(buffer.get(), size, offset);
    DCHECK(ok);
  }

  return rv;
#endif
}

MappedFile* BlockFiles::GetFile(Addr address) {
  DCHECK(thread_checker_->CalledOnValidThread());
  DCHECK(block_files_.size() >= 4);
  DCHECK(address.is_block_file() || !address.is_initialized());
  if (!address.is_initialized())
    return NULL;

  int file_index = address.FileNumber();
  if (static_cast<unsigned int>(file_index) >= block_files_.size() ||
      !block_files_[file_index]) {
    // We need to open the file
    if (!OpenBlockFile(file_index))
      return NULL;
  }
  DCHECK(block_files_.size() >= static_cast<unsigned int>(file_index));
  return block_files_[file_index];
}

bool BlockFiles::GrowBlockFile(MappedFile* file, BlockFileHeader* header) {
  if (kMaxBlocks == header->max_entries)
    return false;

  ScopedFlush flush(file);
  DCHECK(!header->empty[3]);
  int new_size = header->max_entries + 1024;
  if (new_size > kMaxBlocks)
    new_size = kMaxBlocks;

  int new_size_bytes = new_size * header->entry_size + sizeof(*header);

  if (!file->SetLength(new_size_bytes)) {
    // Most likely we are trying to truncate the file, so the header is wrong.
    if (header->updating < 10 && !FixBlockFileHeader(file)) {
      // If we can't fix the file increase the lock guard so we'll pick it on
      // the next start and replace it.
      header->updating = 100;
      return false;
    }
    return (header->max_entries >= new_size);
  }

  FileLock lock(header);
  header->empty[3] = (new_size - header->max_entries) / 4;  // 4 blocks entries
  header->max_entries = new_size;

  return true;
}

MappedFile* BlockFiles::FileForNewBlock(FileType block_type, int block_count) {
  COMPILE_ASSERT(RANKINGS == 1, invalid_file_type);
  MappedFile* file = block_files_[block_type - 1];
  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());

  TimeTicks start = TimeTicks::Now();
  while (NeedToGrowBlockFile(header, block_count)) {
    if (kMaxBlocks == header->max_entries) {
      file = NextFile(file);
      if (!file)
        return NULL;
      header = reinterpret_cast<BlockFileHeader*>(file->buffer());
      continue;
    }

    if (!GrowBlockFile(file, header))
      return NULL;
    break;
  }
  HISTOGRAM_TIMES("DiskCache.GetFileForNewBlock", TimeTicks::Now() - start);
  return file;
}

// Note that we expect to be called outside of a FileLock... however, we cannot
// DCHECK on header->updating because we may be fixing a crash.
bool BlockFiles::FixBlockFileHeader(MappedFile* file) {
  ScopedFlush flush(file);
  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  int file_size = static_cast<int>(file->GetLength());
  if (file_size < static_cast<int>(sizeof(*header)))
    return false;  // file_size > 2GB is also an error.

  const int kMinBlockSize = 36;
  const int kMaxBlockSize = 4096;
  if (header->entry_size < kMinBlockSize ||
      header->entry_size > kMaxBlockSize || header->num_entries < 0)
    return false;

  // Make sure that we survive crashes.
  header->updating = 1;
  int expected = header->entry_size * header->max_entries + sizeof(*header);
  if (file_size != expected) {
    int max_expected = header->entry_size * kMaxBlocks + sizeof(*header);
    if (file_size < expected || header->empty[3] || file_size > max_expected) {
      NOTREACHED();
      LOG(ERROR) << "Unexpected file size";
      return false;
    }
    // We were in the middle of growing the file.
    int num_entries = (file_size - sizeof(*header)) / header->entry_size;
    header->max_entries = num_entries;
  }

  FixAllocationCounters(header);
  int empty_blocks = EmptyBlocks(header);
  if (empty_blocks + header->num_entries > header->max_entries)
    header->num_entries = header->max_entries - empty_blocks;

  if (!ValidateCounters(header))
    return false;

  header->updating = 0;
  return true;
}

// We are interested in the total number of blocks used by this file type, and
// the max number of blocks that we can store (reported as the percentage of
// used blocks). In order to find out the number of used blocks, we have to
// substract the empty blocks from the total blocks for each file in the chain.
void BlockFiles::GetFileStats(int index, int* used_count, int* load) {
  int max_blocks = 0;
  *used_count = 0;
  *load = 0;
  for (;;) {
    if (!block_files_[index] && !OpenBlockFile(index))
      return;

    BlockFileHeader* header =
        reinterpret_cast<BlockFileHeader*>(block_files_[index]->buffer());

    max_blocks += header->max_entries;
    int used = header->max_entries;
    for (int i = 0; i < 4; i++) {
      used -= header->empty[i] * (i + 1);
      DCHECK_GE(used, 0);
    }
    *used_count += used;

    if (!header->next_file)
      break;
    index = header->next_file;
  }
  if (max_blocks)
    *load = *used_count * 100 / max_blocks;
}

}  // namespace disk_cache
