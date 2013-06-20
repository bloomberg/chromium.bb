// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_BLOCK_FILES_H_
#define NET_DISK_CACHE_BLOCK_FILES_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/disk_cache/addr.h"
#include "net/disk_cache/mapped_file.h"

namespace disk_cache {

// This class handles the set of block-files open by the disk cache.
class NET_EXPORT_PRIVATE BlockFiles {
 public:
  explicit BlockFiles(const base::FilePath& path);
  ~BlockFiles();

  // Performs the object initialization. create_files indicates if the backing
  // files should be created or just open.
  bool Init(bool create_files);

  // Creates a new entry on a block file. block_type indicates the size of block
  // to be used (as defined on cache_addr.h), block_count is the number of
  // blocks to allocate, and block_address is the address of the new entry.
  bool CreateBlock(FileType block_type, int block_count, Addr* block_address);

  // Removes an entry from the block files. If deep is true, the storage is zero
  // filled; otherwise the entry is removed but the data is not altered (must be
  // already zeroed).
  void DeleteBlock(Addr address, bool deep);

  // Close all the files and set the internal state to be initializad again. The
  // cache is being purged.
  void CloseFiles();

  // Sends UMA stats.
  void ReportStats();

  // Returns true if the blocks pointed by a given address are currently used.
  // This method is only intended for debugging.
  bool IsValid(Addr address);

 private:
  // Returns the file that stores a given address.
  MappedFile* GetFile(Addr address);

  // Attemp to grow this file. Fails if the file cannot be extended anymore.
  bool GrowBlockFile(MappedFile* file, BlockFileHeader* header);

  // Returns the appropriate file to use for a new block.
  MappedFile* FileForNewBlock(FileType block_type, int block_count);

  // Restores the header of a potentially inconsistent file.
  bool FixBlockFileHeader(MappedFile* file);

  // Retrieves stats for the given file index.
  void GetFileStats(int index, int* used_count, int* load);

  bool init_;

  DISALLOW_COPY_AND_ASSIGN(BlockFiles);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCK_FILES_H_
